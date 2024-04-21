#include <sys/types.h>
#include <iostream>
#include <sys/socket.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "err.h"
#include "common.h"
#include "protconst.h"

using namespace std;

namespace glob {
    int last_socket_fd;
    char last_message[65000];
    ssize_t last_message_length;
    struct sockaddr_in last_address;
    uint8_t last_prot;
}

// Read n bytes from a descriptor. Use in place of read() when fd is a stream socket.
ssize_t readn(int fd, char *vptr, size_t n) {
    ssize_t nleft, nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0)
            return nread;     // When error, return < 0.
        else if (nread == 0)
            break;            // EOF

        nleft -= nread;
        ptr += nread;
    }
    return n - nleft;         // return >= 0
}

// Write n bytes to a descriptor.
ssize_t writen(int fd, char *vptr, size_t n){
    ssize_t nleft, nwritten;
    char *ptr;

    ptr = vptr;               // Can't do pointer arithmetic on void*.
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
            return nwritten;  // error

        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}

void send_message(int socket_fd, char *message, ssize_t message_length, struct sockaddr_in address,
                  uint8_t prot) {
    glob::last_socket_fd = socket_fd;
    memcpy(glob::last_message, message, message_length);
    glob::last_message_length = message_length;
    glob::last_address = address;
    glob::last_prot = prot;
    socklen_t address_length = (socklen_t) sizeof(address);
    if (prot == PROT_TCP) {
        ssize_t sent_length = writen(socket_fd, message, message_length);
        if (sent_length < 0)
            syserr("writen");
        else if ((ssize_t) sent_length != message_length)
            fatal("incomplete writen");
    }
    else {
        ssize_t sent_length = sendto(socket_fd, message, message_length, 0,
                                  (struct sockaddr *) &address, address_length);
        if (sent_length != message_length)
            syserr("sendto");
    }
}

void resend_last_message() {
    send_message(glob::last_socket_fd, glob::last_message, glob::last_message_length,
                 glob::last_address, glob::last_prot);
}


uint16_t read_port(char const *string) {
    char *endptr;
    errno = 0;
    unsigned long port = strtoul(string, &endptr, 10);
    if (errno != 0 || *endptr != 0 || port > UINT16_MAX) {
        fatal("%s is not a valid port number", string);
    }
    return (uint16_t) port;
}

struct sockaddr_in get_server_address(char const *host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *address_result;
    int errcode = getaddrinfo(host, NULL, &hints, &address_result);
    if (errcode != 0) {
        fatal("getaddrinfo: %s", gai_strerror(errcode));
    }

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET;   // IPv4
    send_address.sin_addr.s_addr =       // IP address
            ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr;
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}