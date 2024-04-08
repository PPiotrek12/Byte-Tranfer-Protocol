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


// int receive_CONN(int socket_fd, struct sockaddr_in *client_address) {
//     static char buffer[18];

//     struct sockaddr_in ca;
//     socklen_t address_length = (socklen_t) sizeof(ca);
//     ssize_t length = recvfrom(socket_fd, buffer, 18, 0, (struct sockaddr *) &ca, &address_length);
//     if (length < 0)
//         return 1;

//     // if (length != 18)
//     //     return 1;
    
//     uint8_t type = buffer[0];
//     uint64_t ses_id;
//     memcpy(&ses_id, buffer + 1, 8);
//     uint8_t prot = buffer[9];
//     uint64_t seq_len;
//     memcpy(&seq_len, buffer + 10, 8);

//     if (type != CONN) {
//         return 1;
//     }
//     cout<<"mam\n";
// }


int receive_CONN(int socket_fd, struct sockaddr_in *client_address, uint64_t *ses_id, uint8_t *prot, uint64_t *seq_len) {
    static char buffer[18];
    socklen_t address_length = (socklen_t) sizeof(client_address);
    ssize_t length = recvfrom(socket_fd, buffer, 18, 0, (struct sockaddr *) &client_address, &address_length);
    if (length < 0)
        return 1;
    printf("XDD");
}

void udp_server(struct sockaddr_in server_address) {
    int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0)
        syserr("socket");

    if (bind(socket_fd, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) < 0)
        syserr("bind");
    
    while (true) {
        struct sockaddr_in client_address;
        uint64_t ses_id;
        uint8_t prot;
        uint64_t seq_len;
        if (receive_CONN(socket_fd, &client_address, &ses_id, &prot, &seq_len))
            continue;
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

    // if (protocol == "udp") {
        
    //     udp_server(server_address);
    // }
        
}