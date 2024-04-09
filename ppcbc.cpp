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
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <vector>

using namespace std;



void receive_CON_ACC_RJT(int socket_fd, struct sockaddr_in server_address, uint8_t *type, uint64_t ses_id) {
    static char buffer[9];
    socklen_t address_length = (socklen_t) sizeof(server_address);
    ssize_t length = recvfrom(socket_fd, buffer, 9, 0, (struct sockaddr *) &server_address, &address_length);
    if (length < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) // timeout
            exit(1);
        syserr("recvfrom");
    }
    // if (length != 9) return 1;
    *type = buffer[0];
    if (*type != CONACC && *type != CONRJT)
        fatal("invalid packet type");
    uint64_t res_ses_id;
    memcpy(&res_ses_id, buffer + 1, 8);
    if (ses_id != res_ses_id) 
        fatal("invalid session id");
}

void send_CONN(int socket_fd, struct sockaddr_in server_address, uint64_t ses_id, uint8_t prot, uint64_t seq_len) {
    static char message[18];
    message[0] = CONN;
    memcpy(message + 1, &ses_id, 8);
    message[9] = prot;
    memcpy(message + 10, &seq_len, 8);
    send_message(socket_fd, message, sizeof(message), server_address);
}

void send_DATA(int socket_fd, struct sockaddr_in server_address, uint64_t ses_id, char *data, uint64_t seq_len) {
    uint32_t packet_size = 100; // TODO

    uint64_t already_sent = 0;
    uint64_t packet_nr = 0;
    while (already_sent < seq_len) {
        char message[packet_size + 21];
        message[0] = DATA;
        memcpy(message + 1, &ses_id, 8);
        memcpy(message + 9, &packet_nr, 8);
        uint32_t bytes_nr = min((uint64_t) packet_size, seq_len - already_sent);
        memcpy(message + 17, &bytes_nr, 4);
        mempcpy(message + 21, data, bytes_nr);
        send_message(socket_fd, message, sizeof(message), server_address);
        already_sent += bytes_nr;
        packet_nr++;
    }
}

void udp_client(struct sockaddr_in server_address, char *data, uint64_t seq_len) {
    int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("socket");
    }
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = MAX_WAIT;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)))
        syserr("setsockopt");

    srand(time(NULL));
    uint64_t ses_id = ((long long)rand() << 32) | rand(); // TODO

    send_CONN(socket_fd, server_address, ses_id, PROT_UDP, seq_len);

    uint8_t type = -1;
    receive_CON_ACC_RJT(socket_fd, server_address, &type, ses_id);
    if (type == CONRJT) 
        return;

    send_DATA(socket_fd, server_address, ses_id, data, seq_len);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fatal("usage: %s <protocol> <host> <port>", argv[0]);
    }
    string protocol = argv[1];
    const char *host = argv[2];
    uint16_t port = read_port(argv[3]);
    uint8_t prot = 0;
    if (protocol == "udp")
        prot = PROT_UDP;
    else if (protocol == "tcp")
        prot = PROT_TCP;
    else if (protocol == "udpr")
        prot = PROT_UDPR;
    else
        fatal("Invalid protocol");

    // Reading from input.
    vector <char> vec_input;
    char act;
    do {
        act = getc(stdin);
        vec_input.push_back(act);
    } while (act != EOF);
    u_int64_t seq_len = vec_input.size();
    char input[seq_len];
    for (uint64_t i = 0; i < seq_len; i++)
        input[i] = vec_input[i];

    struct sockaddr_in server_address = get_server_address(host, port);
    if (prot == PROT_UDP)
        udp_client(server_address, input, seq_len);
}