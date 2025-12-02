#include "c2.h"
#include "common.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

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
    
    // Allocate buffer
    void *payload = malloc(payload_len);
    if (!payload) {
        printf("[ERR] failed to allocate %u bytes for payload\n", payload_len);
        return NULL;
    }
    
    // Receive payload
    if (safe_recv(sock, payload, payload_len, flags) <= 0) {
        free(payload);
        return NULL;
    }
    
    if (out_len) {
        *out_len = payload_len;
    }
    
    return payload;
}
