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

int safe_send(int sock, const void *buf, int len, int flags);
int safe_recv(int sock, void *buf, int len, int flags);
int safe_send_payload(int sock, const void *payload, int payload_len, int flags);
void *safe_recv_payload(int sock, int *out_len, int flags);

#endif
