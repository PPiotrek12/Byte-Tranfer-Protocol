#include <sys/types.h>
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

#include <bitset>
#include <stdio.h>
#include <iostream>
#include "err.h"
#include "common.h"
#include <string>
#include "protconst.h"

using namespace std;

void send_CONACC(int socket_fd, struct sockaddr_in client_address, uint64_t ses_id) {
    static char message[9];
    message[0] = CONACC;
    memcpy(message + 1, &ses_id, 8);
    send_message(socket_fd, message, sizeof(message), client_address);
}

void send_CONRJT(int socket_fd, struct sockaddr_in client_address, uint64_t ses_id) {
    static char message[9];
    message[0] = CONRJT;
    memcpy(message + 1, &ses_id, 8);
    send_message(socket_fd, message, sizeof(message), client_address);
}

int receive_CONN(int socket_fd, struct sockaddr_in *client_address, uint8_t *type, uint64_t *ses_id, uint8_t *prot, uint64_t *seq_len) {
    static char buffer[18];
    socklen_t address_length = (socklen_t) sizeof(client_address);
    ssize_t length = recvfrom(socket_fd, buffer, 18, 0, (struct sockaddr *) client_address, &address_length);
    if (length < 0)
        return 1;
    // if (length != 18)
    //     return 1;
    
    *type = buffer[0];
    memcpy(ses_id, buffer + 1, 8);
    *prot = buffer[9];
    memcpy(seq_len, buffer + 10, 8);

    if (*type != CONN)
        return 1;
    return 0;
}

void udp_server(struct sockaddr_in server_address) {
    int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0)
        syserr("socket");
    if (bind(socket_fd, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) < 0)
        syserr("bind");

    while (true) {
        uint8_t prot, type;
        uint64_t seq_len, ses_id;
        fflush(stdout);

        struct sockaddr_in client_address;

        if (receive_CONN(socket_fd, &client_address, &type, &ses_id, &prot, &seq_len))
            continue;
    
        send_CONACC(socket_fd, client_address, ses_id);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fatal("usage: %s <protocol> <port>", argv[0]);
    }
    string protocol = argv[1];
    uint16_t port = read_port(argv[2]);
    uint8_t prot = 0;
    if (protocol == "udp")
        prot = PROT_UDP;
    else if (protocol == "tcp")
        prot = PROT_TCP;
    else if (protocol == "udpr")
        prot = PROT_UDPR;
    else
        fatal("Invalid protocol");
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // Listening on all interfaces.
    server_address.sin_port = htons(port);

    if (prot == PROT_UDP)
        udp_server(server_address);
}