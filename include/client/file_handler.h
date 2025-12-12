#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

void upload_file_to_server(sock_t sock, char* remote_path);
void download_file_from_server(sock_t sock, char *remote_path);

#endif