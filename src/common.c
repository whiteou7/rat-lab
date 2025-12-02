#include "c2.h"
#include "common.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Send exactly len bytes (handles partial sends)
int safe_send(int sock, const void *buf, int len, int flags) {
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
int safe_recv(int sock, void *buf, int len, int flags) {
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

// Send 4-byte length prefix followed by payload
int safe_send_payload(int sock, const void *payload, int payload_len, int flags) {
    // Send length (network byte order)
    uint32_t net_len = htonl((uint32_t)payload_len);
    if (safe_send(sock, &net_len, sizeof(net_len), flags) <= 0) return -1;
    if (payload_len == 0) {
        return 0;
    }
    if (safe_send(sock, payload, payload_len, flags) <= 0) return -1;
    
    return payload_len;
}

// Receive length-prefixed payload (caller must free the returned buffer)
// Returns pointer to malloc'd buffer, or NULL on error
// The received length is stored in *out_len if out_len is not NULL
void *safe_recv_payload(int sock, int *out_len, int flags) {
    // Receive length (network byte order)
    uint32_t net_len;
    if (safe_recv(sock, &net_len, sizeof(net_len), flags) <= 0) return NULL;
    
    uint32_t payload_len = ntohl(net_len);
    
    // Sanity check to prevent excessive allocation
    if (payload_len > 100 * 1024 * 1024) { // 100MB limit
        printf("[ERR] payload too large: %u bytes\n", payload_len);
        return NULL;
    }
    
    // Allocate buffer (ensure non-null even for zero-length payloads)
    void *payload = NULL;
    if (payload_len > 0) {
        payload = malloc(payload_len);
    } else {
        payload = malloc(1);
    }
    if (!payload) {
        printf("[ERR] failed to allocate %u bytes for payload\n", payload_len);
        return NULL;
    }
    
    // Receive payload
    if (payload_len > 0) {
        if (safe_recv(sock, payload, payload_len, flags) <= 0) {
            free(payload);
            return NULL;
        }
    }
    
    if (out_len) {
        *out_len = payload_len;
    }
    
    return payload;
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
