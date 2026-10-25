/* визначення протоколу */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>

#define MSG_ECHO  0x01
#define MSG_TIME  0x02
#define MSG_QUIT  0x03
#define MSG_ERROR 0xFF

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint16_t length;  /* довжина payload у байтах */
} MsgHeader;

#define HEADER_SIZE sizeof(MsgHeader)

static inline ssize_t read_full(int fd, void *buf, size_t n)
{
    size_t total = 0;
    char *ptr = buf;
    while (total < n) {
        ssize_t r = read(fd, ptr + total, n - total);
        if (r == 0) return 0; /* EOF */
        if (r == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += r;
    }
    return total;
}

static inline int send_message(int fd, uint8_t type, const void *payload, uint16_t len)
{
    MsgHeader hdr;
    hdr.type = type;
    hdr.length = htons(len);

    size_t total_len = HEADER_SIZE + len;
    char *packet = malloc(total_len);
    if (!packet) return -1;

    memcpy(packet, &hdr, HEADER_SIZE);
    if (len > 0 && payload) {
        memcpy(packet + HEADER_SIZE, payload, len);
    }

    size_t sent = 0;
    while (sent < total_len) {
        ssize_t w = write(fd, packet + sent, total_len - sent);
        if (w == -1) {
            if (errno == EINTR) continue;
            free(packet);
            return -1;
        }
        sent += w;
    }

    free(packet);
    return 0;
}

static inline int recv_message(int fd, MsgHeader *hdr, void *payload, size_t max_len)
{
    ssize_t r = read_full(fd, hdr, HEADER_SIZE);
    if (r <= 0) return r;

    hdr->length = ntohs(hdr->length);
    if (hdr->length > max_len) {
        return -1; /* Завеликий payload */
    }

    if (hdr->length > 0) {
        r = read_full(fd, payload, hdr->length);
        if (r <= 0) return r;
    }
    return 0;
}

#endif
