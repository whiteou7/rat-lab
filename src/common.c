#include "c2.h"
#include "common.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Send exactly len bytes (handles partial sends)
int send_all(int sock, const void *buf, int len, int flags) {
    int total_sent = 0;
    const char *ptr = (const char *)buf;
    
    while (total_sent < len) {
        int r = send(sock, ptr + total_sent, len - total_sent, flags);
        if (IS_ERROR(r)) {
            printf("[ERR] send failed: %d\n", GET_ERR());
            return -1;
        } else if (r == 0) {
            printf("[INFO] peer closed connection during send\n");
            return 0;
        }
        total_sent += r;
    }
    return total_sent;
}

// Receive exactly len bytes (handles partial receives)
int recv_all(int sock, void *buf, int len, int flags) {
    int total_recv = 0;
    char *ptr = (char *)buf;
    
    while (total_recv < len) {
        int r = recv(sock, ptr + total_recv, len - total_recv, flags);
        if (IS_ERROR(r)) {
            printf("[ERR] recv failed: %d\n", GET_ERR());
            return -1;
        } else if (r == 0) {
            printf("[INFO] peer closed connection during recv\n");
            return 0;
        }
        total_recv += r;
    }
    return total_recv;
}

// Send payload with length and cmd as header
int safe_send(int sock, const void *payload, int cmd, int payload_len, int flags) {
    // Send length (network byte order)
    uint32_t net_len = htonl((uint32_t)payload_len);
    uint32_t net_cmd = htonl((uint32_t)cmd);
    if (send_all(sock, &net_cmd, sizeof(net_cmd), flags) <= 0) return -1;
    if (send_all(sock, &net_len, sizeof(net_len), flags) <= 0) return -1;
    if (payload_len == 0) {
        return 0;
    }
    if (send_all(sock, payload, payload_len, flags) <= 0) return -1;
    
    return payload_len;
}

// Returns allocated buffer with payload (caller must free), or NULL on error / zero-length payload.
// If out_cmd or out_len are non-NULL they will be set.
void *safe_recv(int sock, int *out_cmd, int *out_len, int flags) {
    uint32_t net_cmd = 0;
    uint32_t net_len = 0;
    int r;

    // Receive command (network order)
    r = recv_all(sock, &net_cmd, sizeof(net_cmd), flags);
    if (r <= 0) return NULL;
    int cmd = (int)ntohl(net_cmd);

    // Receive length
    r = recv_all(sock, &net_len, sizeof(net_len), flags);
    if (r <= 0) return NULL;
    int len = (int)ntohl(net_len);

    // If caller wants these values, set them
    if (out_cmd) *out_cmd = cmd;
    if (out_len) *out_len = len;

    // If length is zero, nothing to return
    if (len == 0) return NULL;

    // Allocate buffer
    void *buf = malloc((size_t)len);
    if (!buf) {
        printf("[ERR] malloc failed\n");
        return NULL;
    }

    // Receive payload
    r = recv_all(sock, buf, len, flags);
    if (r <= 0) {
        free(buf);
        return NULL;
    }

    return buf;
}



// Read entire file into memory. Caller must free the returned buffer.
char* read_file(const char* path, size_t* size) {
    if (!path) {
        return NULL;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("[ERR] failed to open file %s: %s\n", path, strerror(errno));
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        printf("[ERR] fseek failed for %s\n", path);
        fclose(fp);
        return NULL;
    }

    long length = ftell(fp);
    if (length < 0) {
        printf("[ERR] ftell failed for %s\n", path);
        fclose(fp);
        return NULL;
    }
    rewind(fp);

    size_t file_size = (size_t)length;
    char *buffer = NULL;
    if (file_size > 0) {
        buffer = (char*)malloc(file_size);
    } else {
        buffer = (char*)malloc(1);
    }

    if (!buffer) {
        printf("[ERR] failed to allocate %zu bytes for %s\n", file_size, path);
        fclose(fp);
        return NULL;
    }

    if (file_size > 0) {
        size_t read = fread(buffer, 1, file_size, fp);
        if (read != file_size) {
            printf("[ERR] fread failed for %s\n", path);
            free(buffer);
            fclose(fp);
            return NULL;
        }
    }

    fclose(fp);

    if (size) {
        *size = file_size;
    }

    return buffer;
}
