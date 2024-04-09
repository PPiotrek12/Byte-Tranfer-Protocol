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
const int DATA_MAX_SIZE = 64000;

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

void send_RCVD(int socket_fd, struct sockaddr_in client_address, uint64_t ses_id) {
    static char message[9];
    message[0] = RCVD;
    memcpy(message + 1, &ses_id, 8);
    send_message(socket_fd, message, sizeof(message), client_address);
}

void receive_CONN(int socket_fd, struct sockaddr_in *client_address, uint64_t *ses_id, uint8_t *prot, uint64_t *seq_len) {
    static char buffer[18];
    while (true) {
        socklen_t address_length = (socklen_t) sizeof(client_address);
        ssize_t length = recvfrom(socket_fd, buffer, 18, 0, (struct sockaddr *) client_address, &address_length);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) // timeout
                continue;
            syserr("recvfrom");
        }
        // if (length != 18) continue;
        uint8_t res_type = buffer[0];
        uint8_t res_prot = buffer[9];
        if (res_type != CONN) {
            err("invalid packet type");
            continue;
        }
        if (res_prot != PROT_UDP && res_prot != PROT_UDPR) {
            err("invalid protocol");
            continue;
        }
        break;
    }

    // Here we have received correct CONN packet.
    memcpy(ses_id, buffer + 1, 8);
    *prot = buffer[9];
    memcpy(seq_len, buffer + 10, 8);
}

// Returns 1 if there is need to close connection.
int receive_DATA(int socket_fd, uint64_t ses_id, uint8_t prot, uint64_t seq_len, char *data) {
    uint64_t already_read = 0;
    uint64_t last_packet_nr = 0;
    while (already_read < seq_len) {
        // Receiving packet.
        static char buffer[64000+21];
        uint64_t res_ses_id, res_packet_nr;
        uint32_t res_bits_nr;
        uint8_t res_type;

        while (true) {
            struct sockaddr_in res_address;
            socklen_t address_length = (socklen_t) sizeof(res_address);
            ssize_t length = recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *) &res_address, &address_length);
            if (length < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) // timeout
                    return 1;
                syserr("recvfrom");
            }

            res_type = buffer[0];
            memcpy(&res_ses_id, buffer + 1, 8);
            memcpy(&res_packet_nr, buffer + 9, 8);
            memcpy(&res_bits_nr, buffer + 17, 4);

            if (res_type != DATA) {
                err("invalid packet type");
                return 1;
            }
            if (res_ses_id != ses_id) {
                err("invalid session id");
                return 1;
            }
            if (already_read > 0 && res_packet_nr != last_packet_nr + 1) {
                if (res_packet_nr < last_packet_nr + 1) continue;
                err("invalid packet number");
                return 1;
            }
            break;
        }

        // Here we have received correct DATA packet.
        memcpy(data + already_read, buffer + 21, res_bits_nr);
        already_read += res_bits_nr;
        last_packet_nr = res_packet_nr;
    }
    return 0;
}

void udp_server(struct sockaddr_in server_address) {
    int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0)
        syserr("socket");
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = MAX_WAIT;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)))
        syserr("setsockopt");
    if (bind(socket_fd, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) < 0)
        syserr("bind");

    while (true) {
        uint8_t prot;
        uint64_t seq_len, ses_id;
        struct sockaddr_in client_address;
        receive_CONN(socket_fd, &client_address, &ses_id, &prot, &seq_len);
        send_CONACC(socket_fd, client_address, ses_id);

        static char data[DATA_MAX_SIZE];
        if (receive_DATA(socket_fd, ses_id, prot, seq_len, data))
            continue; // Next client.
        printf("%s", data);

        send_RCVD(socket_fd, client_address, ses_id);
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