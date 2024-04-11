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

#include "tcp_client_lib.h"
#include "err.h"
#include "common.h"
#include "protconst.h"

using namespace std;

void send_CONN_tcp(int socket_fd, struct sockaddr_in server_address, uint64_t ses_id, uint64_t seq_len) {
    static char message[CONN_LEN];
    message[0] = CONN;
    memcpy(message + 1, &ses_id, 8);
    message[9] = PROT_TCP;
    memcpy(message + 10, &seq_len, 8);
    send_message(socket_fd, message, sizeof(message), server_address, PROT_TCP);
}

void receive_CON_ACC_tcp(int socket_fd, uint64_t ses_id) {
    static char buffer[CONACC_LEN];
    while (true) {
        ssize_t length = readn(socket_fd, buffer, CONACC_LEN);
        if (length < 0) {
            close(socket_fd);
            if (errno == EAGAIN || errno == EWOULDBLOCK) // Timeout.
                fatal("could not receive packet");
            syserr("readn");
        }
        uint64_t res_ses_id;
        uint8_t  type = buffer[0];
        memcpy(&res_ses_id, buffer + 1, 8);
        if (type != CONACC) {
            close(socket_fd);
            fatal("invalid packet type");
        }
        if (ses_id != res_ses_id) {
            close(socket_fd);
            fatal("invalid session id");
        }
        if (length != CONRJT_LEN) {
            close(socket_fd);
            fatal("invalid packet length");
        }
        break;
    }
}

void send_one_DATA_packet_tcp(int socket_fd, struct sockaddr_in server_address, 
                          uint64_t ses_id, uint64_t packet_nr, uint32_t bytes_nr,
                          char *data, uint64_t already_sent) {
    char message[DATA_PACKET_SIZE + 21];
    message[0] = DATA;
    memcpy(message + 1, &ses_id, 8);
    memcpy(message + 9, &packet_nr, 8);
    memcpy(message + 17, &bytes_nr, 4);
    mempcpy(message + 21, data + already_sent, bytes_nr);
    send_message(socket_fd, message, sizeof(message), server_address, PROT_TCP);
}

void send_DATA_tcp(int socket_fd, struct sockaddr_in server_address, uint64_t ses_id, char *data, uint64_t seq_len) {
    uint64_t already_sent = 0;
    uint64_t packet_nr = 0;
    while (already_sent < seq_len) {
        uint32_t bytes_nr = min((uint64_t) DATA_PACKET_SIZE, seq_len - already_sent);
        send_one_DATA_packet_tcp(socket_fd, server_address, ses_id, packet_nr, bytes_nr, data, already_sent);
        already_sent += bytes_nr;
        packet_nr++;
    }
}

void receive_RCVD_RJT_tcp(int socket_fd, uint64_t ses_id) {
    while (true) {
        static char buffer[RJT_LEN];
        ssize_t length1 = readn(socket_fd, buffer, RCVD_LEN);
        if (length1 < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // Timeout.
                close(socket_fd);
                fatal("could not receive packet");
            }
            close(socket_fd);
            syserr("readn");
        }
        uint8_t res_type = buffer[0];

        uint64_t res_ses_id;
        memcpy(&res_ses_id, buffer + 1, 8);
        if (ses_id != res_ses_id) {
            close(socket_fd);
            fatal("invalid session id");
        }
        if (res_type == RCVD) {
            if (length1 != RCVD_LEN) {
                close(socket_fd);
                fatal("invalid packet length");
            }
            return;
        }
        else if (res_type == RJT) {
            ssize_t length2 = readn(socket_fd, buffer, RJT_LEN - RCVD_LEN);
            if (length2 < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) { // Timeout.
                    close(socket_fd);
                    fatal("could not receive packet");
                }
                close(socket_fd);
                syserr("readn");
            }
            if (length1 + length2 < RJT_LEN) {
                close(socket_fd);
                fatal("invalid packet length");
            }
            uint64_t res_packet_nr;
            memcpy(&res_packet_nr, buffer, 8);
            close(socket_fd);
            fatal("packet number %ld was rejected", res_packet_nr);
        }
        else {
            close(socket_fd);
            fatal("invalid packet type");
        }
        break;
    }
}

void tcp_client(struct sockaddr_in server_address, char *data, uint64_t seq_len) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        syserr("socket");
    }
    struct timeval timeout;
    timeout.tv_sec = MAX_WAIT;
    timeout.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)))
        syserr("setsockopt");
    if (connect(socket_fd, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) < 0)
        syserr("cannot connect to the server");

    srand(time(NULL));
    uint64_t ses_id = ((long long)rand() << 32) | rand(); // TODO

    send_CONN_tcp(socket_fd, server_address, ses_id, seq_len);

    receive_CON_ACC_tcp(socket_fd, ses_id);
    send_DATA_tcp(socket_fd, server_address, ses_id, data, seq_len);
    receive_RCVD_RJT_tcp(socket_fd, ses_id);
}
