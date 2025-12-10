#ifndef COMMON_H
#define COMMON_H

#ifdef _WIN32
#include <winsock2.h>
#define GET_ERR() WSAGetLastError()
#define IS_ERROR(ret) ((ret) == SOCKET_ERROR)
#else
#include <errno.h>
#define GET_ERR() errno
#define IS_ERROR(ret) ((ret) == -1)
#endif

int safe_send(int sock, const void *payload, int cmd, int payload_len, int flags);
void *safe_recv(int sock, int *out_cmd, int *out_len, int flags);
char* read_file(const char* path, size_t* size);

#endif
