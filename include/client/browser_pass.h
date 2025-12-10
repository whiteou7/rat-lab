#ifndef PASS_H
#define PASS_H
#include <windows.h>

char* get_master_key();
int decrypt_password(BYTE* encrypted, int enc_len, char* master_key, char* out_password, int out_size);
char* get_browser_password();

#endif