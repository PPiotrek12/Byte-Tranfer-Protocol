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
#include <string>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <vector>

#include "udp_client_lib.h"
#include "err.h"
#include "common.h"
#include "protconst.h"

using namespace std;


/* ==================================== RECEIVE FUNCTIONS ====================================== */

void receive_CON_ACC_RJT(int socket_fd, struct sockaddr_in server_address, uint8_t *type, 
                         uint64_t ses_id, int retransmits) {
    static char buffer[CONRJT_LEN];
    socklen_t address_length = (socklen_t) sizeof(server_address);
    while (true) {
        ssize_t length = recvfrom(socket_fd, buffer, CONRJT_LEN, 0, (struct sockaddr *) &server_address, &address_length);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // Timeout.
                if (retransmits > 0) {
                    resend_last_message();
                    retransmits--;
                    continue;
                }
                else
                    fatal("could not receive packet");
            }
            syserr("recvfrom");
        }
        uint64_t res_ses_id;
        *type = buffer[0];
        memcpy(&res_ses_id, buffer + 1, 8);        
        if (ses_id != res_ses_id) 
            fatal("invalid session id");
        if (length != CONRJT_LEN)
            fatal("invalid packet length");
        if (*type != CONACC && *type != CONRJT)
            fatal("invalid packet type");
        break;
    }
}

void receive_ACC_RJT(int socket_fd, struct sockaddr_in server_address, uint8_t *type,
                     uint64_t ses_id, int retransmits, uint64_t packet_nr) {
    static char buffer[ACC_LEN];
    socklen_t address_length = (socklen_t) sizeof(server_address);
    while (true) {
        ssize_t length = recvfrom(socket_fd, buffer, ACC_LEN, 0, (struct sockaddr *) &server_address, &address_length);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // Timeout.
                if (retransmits > 0) {
                    resend_last_message();
                    retransmits--;
                    continue;
                }
                else
                    fatal("could not receive packet");
            }
            syserr("recvfrom");
        }
        uint64_t res_ses_id, res_packet_nr;
        *type = buffer[0];
        memcpy(&res_ses_id, buffer + 1, 8);
        memcpy(&res_packet_nr, buffer + 9, 8);
        if (ses_id != res_ses_id) 
            fatal("invalid session id");
        if (length != ACC_LEN)
            fatal("invalid packet length");
        if (packet_nr != res_packet_nr) {
            if (*type == ACC && res_packet_nr < packet_nr)
                continue; // Ignore packet.
            fatal("invalid packet number");
        }
        if (*type != ACC && *type != RJT) {
            if (res_ses_id == ses_id && *type == CONACC)
                continue; // Ignore packet.
            fatal("invalid packet type");
        }
        break;
    }
}

void receive_RCVD_RJT(int socket_fd, struct sockaddr_in server_address, 
                      uint64_t ses_id, int retransmits) {
    while (true) {
        static char buffer[RJT_LEN];
        socklen_t address_length = (socklen_t) sizeof(server_address);
        ssize_t length = recvfrom(socket_fd, buffer, RJT_LEN, 0, (struct sockaddr *) &server_address, &address_length);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // Timeout.
                if (retransmits > 0) {
                    resend_last_message();
                    retransmits--;
                    continue;
                }
                else
                    fatal("could not receive packet");
            }
            syserr("recvfrom");
        }
        uint64_t res_ses_id;
        uint8_t res_type = buffer[0];
        memcpy(&res_ses_id, buffer + 1, 8);
        if (ses_id != res_ses_id) 
            fatal("invalid session id");
        if (res_type == RCVD) {
            if (length != RCVD_LEN)
                fatal("invalid packet length");
            return;
        }
        else if (res_type == RJT) {
            if (length < RJT_LEN)
                fatal("invalid packet length");
            uint64_t res_packet_nr;
            memcpy(&res_packet_nr, buffer + 9, 8);
            fatal("packet number %ld was rejected", res_packet_nr);
        }
        else if (res_type == CONACC || res_type == ACC)
            continue; // Ingore packet.
        else
            fatal("invalid packet type");
        break;
    }
}


/* ======================================= SEND FUNCTIONS ====================================== */

void send_CONN(int socket_fd, struct sockaddr_in server_address, uint64_t ses_id, uint8_t prot, uint64_t seq_len) {
    static char message[CONN_LEN];
    message[0] = CONN;
    memcpy(message + 1, &ses_id, 8);
    message[9] = prot;
    memcpy(message + 10, &seq_len, 8);
    send_message(socket_fd, message, sizeof(message), server_address, prot);
}

void send_one_DATA_packet(int socket_fd, struct sockaddr_in server_address, 
                          uint64_t ses_id, uint64_t packet_nr, uint32_t bytes_nr,
                          char *data, uint64_t already_sent, uint8_t prot) {
    char message[DATA_PACKET_SIZE + 21];
    message[0] = DATA;
    memcpy(message + 1, &ses_id, 8);
    memcpy(message + 9, &packet_nr, 8);
    memcpy(message + 17, &bytes_nr, 4);
    mempcpy(message + 21, data + already_sent, bytes_nr);
    send_message(socket_fd, message, sizeof(message), server_address, prot);
}

void send_DATA(int socket_fd, struct sockaddr_in server_address, uint64_t ses_id, char *data, int retransmits, uint8_t prot, uint64_t seq_len) {
    uint64_t already_sent = 0;
    uint64_t packet_nr = 0;
    while (already_sent < seq_len) {
        uint32_t bytes_nr = min((uint64_t) DATA_PACKET_SIZE, seq_len - already_sent);
        send_one_DATA_packet(socket_fd, server_address, ses_id, packet_nr, bytes_nr, data, already_sent, prot);

        if (prot == PROT_UDPR) {
            uint8_t type;
            receive_ACC_RJT(socket_fd, server_address, &type, ses_id, retransmits, packet_nr);
            if (type == RJT)
                fatal("packet number %ld was rejected", packet_nr);
        }

        already_sent += bytes_nr;
        packet_nr++;
    }
}


/* ======================================= CLIENT FUNCTIONS ==================================== */

void udp_client(struct sockaddr_in server_address, char *data, uint64_t seq_len, uint8_t prot) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("socket");
    }
    struct timeval timeout;
    timeout.tv_sec = MAX_WAIT;
    timeout.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)))
        syserr("setsockopt");

    srand(time(NULL));
    uint64_t ses_id = ((long long)rand() << 32) | rand(); // TODO

    send_CONN(socket_fd, server_address, ses_id, prot, seq_len);

    uint8_t type = -1;
    int retransmit = 0;
    if (prot == PROT_UDPR) retransmit = MAX_RETRANSMITS;

    receive_CON_ACC_RJT(socket_fd, server_address, &type, ses_id, retransmit);
    if (type == CONRJT) 
        return;

    send_DATA(socket_fd, server_address, ses_id, data, retransmit, prot, seq_len);
    
    receive_RCVD_RJT(socket_fd, server_address, ses_id, retransmit);
}