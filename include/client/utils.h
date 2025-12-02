#ifndef UTILS_H
#define UTILS_H

#include "c2.h"

void psh_receive(sock_t sock);
void upload_file_to_server(sock_t sock);
void download_file_from_server(sock_t sock);

#endif /* UTILS_H */
