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

#include <stdio.h>
#include <iostream>
#include "err.h"
#include "common.h"
#include <string>
#include "protconst.h"
#include "tcp_server_lib.h"

using namespace std;

const int QUEUE_LENGTH = 10;


// Returns: 0 - correct; 1 - next client (close connection); 2 - ignore packet.
int check_received_DATA_packet(int socket_fd, struct sockaddr_in res_address, uint64_t ses_id, 
                                uint64_t seq_len, ssize_t read_length,  uint64_t already_read, 
                                uint64_t last_packet_nr, uint8_t res_type, uint64_t res_ses_id,
                                uint64_t res_packet_nr,  uint32_t res_bytes_nr, uint8_t prot) {

    if (res_type == CONN) { // Someone is trying to interrupt.
        if (res_ses_id == ses_id)
            return 2; // Ignoring CONN sent twice.
        send_CONRJT(socket_fd, res_address, res_ses_id, prot); 
    }
    if (res_ses_id != ses_id) { // Client authorisation.
        err("invalid session id");
        return 2; // Ignore packet.
    }
    if (res_type != DATA) { // Correct client send invalid packet - close connection.
        err("invalid packet type");
        return 1; // Next client.
    }
    if (read_length < MIN_DATA_LEN) { // Correct client send invalid packet.
        err("invalid packet length");
        send_RJT(socket_fd, res_address, res_ses_id, res_packet_nr, prot);
        return 1; // Next client.
    }
    if (res_packet_nr != last_packet_nr + 1) {
        if (res_packet_nr < last_packet_nr + 1)
            return 2; // Ignore packet.
        err("invalid packet number");
        send_RJT(socket_fd, res_address, res_ses_id, res_packet_nr, prot);
        return 1; // Next client.
    }
    if (already_read + res_bytes_nr > seq_len) {
        err("invalid bytes number");
        send_RJT(socket_fd, res_address, res_ses_id, res_packet_nr, prot);
        return 1; // Next client.
    }
    return 0;
}

void receive_CONN_tcp(int client_fd, uint64_t *ses_id, uint8_t *prot, uint64_t *seq_len) {
    static char buffer[CONN_LEN];
    while (true) {
        ssize_t length = readn(client_fd, buffer, CONN_LEN);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) // Timeout.
                continue;
            close(client_fd);
            syserr("readn");
        }
        uint8_t res_type = buffer[0];
        uint8_t res_prot = buffer[9];
        if (length != CONN_LEN) {
            close(client_fd);
            err("invalid packet length");
            continue; // Next client.
        }
        if (res_type != CONN) {
            close(client_fd);
            err("invalid packet type");
            continue; // Next client.
        }
        if (res_prot != PROT_TCP) {
            close(client_fd);
            err("invalid protocol");
            continue; // Next client.
        }
        break;
    }
    // Here we have received correct CONN packet.
    memcpy(ses_id, buffer + 1, 8);
    *prot = buffer[9];
    memcpy(seq_len, buffer + 10, 8);
}

void send_CONACC_tcp(int client_fd, struct sockaddr_in client_address, uint64_t ses_id) {
    static char message[CONACC_LEN];
    message[0] = CONACC;
    memcpy(message + 1, &ses_id, 8);
    send_message(client_fd, message, sizeof(message), client_address, PROT_TCP);
}

// Returns 1 if there is need to close connection.
int receive_one_DATA_packet_tcp(int client_fd, uint64_t last_packet_nr,
                            uint64_t seq_len, char *buffer, uint64_t already_read,
                            uint64_t ses_id, uint64_t *res_packet_nr, uint32_t *res_bytes_nr) {
    while (true) {
        ssize_t length1 = readn(client_fd, buffer, MIN_DATA_LEN - 1); // Reading packet header.
        if (length1 < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // Timeout.
                close(client_fd);
                err("could not receive packet");
                return 1; // Next client.
            }
            syserr("readn");
        }
        uint64_t res_ses_id;
        uint8_t res_type = buffer[0];
        memcpy(&res_ses_id, buffer + 1, 8);
        memcpy(res_packet_nr, buffer + 9, 8);
        memcpy(res_bytes_nr, buffer + 17, 4);

        ssize_t length2 = readn(client_fd, buffer, *res_bytes_nr); // Reading data.
        if (length2 < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // Timeout.
                close(client_fd);
                err("could not receive packet");
                return 1; // Next client.
            }   
            syserr("readn");
        }
        struct sockaddr_in null;
        int res = check_received_DATA_packet(client_fd, null, ses_id, seq_len, length1 + length2,
                                             already_read, last_packet_nr, res_type, res_ses_id,
                                             *res_packet_nr, *res_bytes_nr, PROT_TCP);
        if (res == 1) {
            close(client_fd);
            return 1;
        }
        if (res == 2) continue; // Ignore packet.
        break;
    }
    return 0;
}

// Returns 1 if there is need to close connection.
int receive_DATA_tcp(int client_fd, uint64_t ses_id, uint64_t seq_len, char *data) {
    uint64_t already_read = 0, last_packet_nr = -1;
    while (already_read < seq_len) {
        // Receiving packet.
        static char buffer[DATA_MAX_SIZE + MIN_DATA_LEN];
        uint64_t res_packet_nr;
        uint32_t res_bytes_nr;

        if (receive_one_DATA_packet_tcp(client_fd, last_packet_nr, seq_len, buffer, 
                                        already_read, ses_id, &res_packet_nr, &res_bytes_nr))
            return 1;
        // Here we have received correct DATA packet.
        memcpy(data + already_read, buffer, res_bytes_nr);
        already_read += res_bytes_nr;
        last_packet_nr = res_packet_nr;
    }
    return 0;
}

void tcp_server(struct sockaddr_in server_address) {
    signal(SIGPIPE, SIG_IGN);
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
        syserr("socket");
    struct timeval timeout;
    timeout.tv_sec = MAX_WAIT;
    timeout.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)))
        syserr("setsockopt");
    if (bind(socket_fd, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address))<0)
        syserr("bind");
    if (listen(socket_fd, QUEUE_LENGTH) < 0)
        syserr("listen");
    while (true) {
        struct sockaddr_in client_address;
        socklen_t address_length = (socklen_t) sizeof(client_address);
        int client_fd = accept(socket_fd, (struct sockaddr *) &client_address, &address_length);
        if (client_fd < 0)
            syserr("accept");

        uint8_t prot;
        uint64_t seq_len, ses_id;
        receive_CONN_tcp(client_fd, &ses_id, &prot, &seq_len);
        
        send_CONACC_tcp(client_fd, client_address, ses_id);

        static char data[DATA_MAX_SIZE];
        if (receive_DATA_tcp(client_fd, ses_id, seq_len, data))
            continue; // Next client.
        printf("%s", data);
        fflush(stdout);

        send_RCVD(client_fd, client_address, ses_id, PROT_TCP);
    }
}