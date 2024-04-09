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
    char *last_message;
    ssize_t last_message_length;
    struct sockaddr_in last_address;
}

void send_message(int socket_fd, char *message, ssize_t message_length, struct sockaddr_in address) {
    glob::last_socket_fd = socket_fd;
    glob::last_message = message;
    glob::last_message_length = message_length;
    glob::last_address = address;
    socklen_t address_length = (socklen_t) sizeof(address);
    ssize_t sent_length = sendto(socket_fd, message, message_length, 0,
                                  (struct sockaddr *) &address, address_length);
    if (sent_length != message_length) {
        syserr("sendto");
    }
}

void resend_last_message() {
    send_message(glob::last_socket_fd, glob::last_message, glob::last_message_length, glob::last_address);
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