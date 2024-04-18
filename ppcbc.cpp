#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <endian.h>
#include <random>

#include <ctime>
#include <string>
#include <vector>

#include "common.h"
#include "err.h"
#include "protconst.h"

using namespace std;

const uint64_t DATA_PACKET_SIZE = 64000;  // TODO

/* ==================================== COMMON FUNCTIONS ======================================= */

void send_CONN(int socket_fd, struct sockaddr_in server_address, uint64_t ses_id, uint8_t prot,
               uint64_t seq_len) {
    static char message[CONN_LEN];
    message[0] = CONN;
    memcpy(message + 1, &ses_id, 8);
    message[9] = prot;
    seq_len = htobe64(seq_len);
    memcpy(message + 10, &seq_len, 8);
    send_message(socket_fd, message, sizeof(message), server_address, prot);
}

void send_one_DATA_packet(int socket_fd, struct sockaddr_in server_address, uint64_t ses_id,
                          uint64_t packet_nr, uint32_t bytes_nr, char *data, uint64_t already_sent,
                          uint8_t prot) {
    char message[DATA_PACKET_SIZE + 21];
    message[0] = DATA;
    memcpy(message + 1, &ses_id, 8);
    packet_nr = htobe64(packet_nr);
    memcpy(message + 9, &packet_nr, 8);
    uint32_t bytes_nr_n = htobe32(bytes_nr);
    memcpy(message + 17, &bytes_nr_n, 4);
    mempcpy(message + 21, data + already_sent, bytes_nr);
    send_message(socket_fd, message, sizeof(message), server_address, prot);
}

uint64_t get_random() {
    random_device rd;
    mt19937_64 eng(rd());
    uniform_int_distribution<long long> distr(0, ULLONG_MAX/2ULL);
    return distr(eng);
}

/* ======================================= UDP FUNCTIONS ======================================= */

void receive_CON_ACC_RJT_udp(int socket_fd, struct sockaddr_in server_address, uint8_t *type,
                             uint64_t ses_id, int retransmits) {
    static char buffer[CONRJT_LEN];
    socklen_t address_length = (socklen_t)sizeof(server_address);
    while (true) {
        ssize_t length = recvfrom(socket_fd, buffer, CONRJT_LEN, 0,
                                  (struct sockaddr *)&server_address, &address_length);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {  // Timeout.
                if (retransmits > 0) {
                    resend_last_message();
                    retransmits--;
                    continue;
                } else
                    fatal("could not receive packet");
            }
            syserr("recvfrom");
        }
        uint64_t res_ses_id;
        *type = buffer[0];
        memcpy(&res_ses_id, buffer + 1, 8);
        if (ses_id != res_ses_id) fatal("invalid session id");
        if (length != CONRJT_LEN) fatal("invalid packet length");
        if (*type != CONACC && *type != CONRJT) fatal("invalid packet type");
        break;
    }
}

void receive_ACC_RJT_udp(int socket_fd, struct sockaddr_in server_address, uint8_t *type,
                         uint64_t ses_id, int retransmits, uint64_t packet_nr) {
    static char buffer[ACC_LEN];
    socklen_t address_length = (socklen_t)sizeof(server_address);
    while (true) {
        ssize_t length = recvfrom(socket_fd, buffer, ACC_LEN, 0, (struct sockaddr *)&server_address,
                                  &address_length);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {  // Timeout.
                if (retransmits > 0) {
                    resend_last_message();
                    retransmits--;
                    continue;
                } else
                    fatal("could not receive packet");
            }
            syserr("recvfrom");
        }
        uint64_t res_ses_id, res_packet_nr;
        *type = buffer[0];
        memcpy(&res_ses_id, buffer + 1, 8);
        memcpy(&res_packet_nr, buffer + 9, 8);
        res_packet_nr = be64toh(res_packet_nr);
        if (ses_id != res_ses_id) fatal("invalid session id");
        if (length != ACC_LEN) fatal("invalid packet length");
        if (packet_nr != res_packet_nr) {
            if (*type == ACC && res_packet_nr < packet_nr) continue;  // Ignore packet.
            fatal("invalid packet number");
        }
        if (*type != ACC && *type != RJT) {
            if (res_ses_id == ses_id && *type == CONACC) continue;  // Ignore packet.
            fatal("invalid packet type");
        }
        break;
    }
}

void receive_RCVD_RJT_udp(int socket_fd, struct sockaddr_in server_address, uint64_t ses_id,
                          int retransmits) {
    while (true) {
        static char buffer[RJT_LEN];
        socklen_t address_length = (socklen_t)sizeof(server_address);
        ssize_t length = recvfrom(socket_fd, buffer, RJT_LEN, 0, (struct sockaddr *)&server_address,
                                  &address_length);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {  // Timeout.
                if (retransmits > 0) {
                    resend_last_message();
                    retransmits--;
                    continue;
                } else
                    fatal("could not receive packet");
            }
            syserr("recvfrom");
        }
        uint64_t res_ses_id;
        uint8_t res_type = buffer[0];
        memcpy(&res_ses_id, buffer + 1, 8);
        if (ses_id != res_ses_id) fatal("invalid session id");
        if (res_type == RCVD) {
            if (length != RCVD_LEN) fatal("invalid packet length");
            return;
        } else if (res_type == RJT) {
            if (length < RJT_LEN) fatal("invalid packet length");
            uint64_t res_packet_nr;
            memcpy(&res_packet_nr, buffer + 9, 8);
            res_packet_nr = be64toh(res_packet_nr);
            fatal("packet number %ld was rejected", res_packet_nr);
        } else if (res_type == CONACC || res_type == ACC)
            continue;  // Ingore packet.
        else
            fatal("invalid packet type");
        break;
    }
}

void send_DATA_udp(int socket_fd, struct sockaddr_in server_address, uint64_t ses_id, char *data,
                   int retransmits, uint8_t prot, uint64_t seq_len) {
    uint64_t already_sent = 0;
    uint64_t packet_nr = 0;
    while (already_sent < seq_len) {
        uint32_t bytes_nr = min((uint64_t)DATA_PACKET_SIZE, seq_len - already_sent);
        send_one_DATA_packet(socket_fd, server_address, ses_id, packet_nr, bytes_nr, data,
                             already_sent, prot);

        if (prot == PROT_UDPR) {
            uint8_t type;
            receive_ACC_RJT_udp(socket_fd, server_address, &type, ses_id, retransmits, packet_nr);
            if (type == RJT) fatal("packet number %ld was rejected", packet_nr);
        }

        already_sent += bytes_nr;
        packet_nr++;
    }
}

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
    uint64_t ses_id = get_random();

    send_CONN(socket_fd, server_address, ses_id, prot, seq_len);

    uint8_t type = -1;
    int retransmit = 0;
    if (prot == PROT_UDPR) retransmit = MAX_RETRANSMITS;

    receive_CON_ACC_RJT_udp(socket_fd, server_address, &type, ses_id, retransmit);
    if (type == CONRJT) return;

    send_DATA_udp(socket_fd, server_address, ses_id, data, retransmit, prot, seq_len);

    receive_RCVD_RJT_udp(socket_fd, server_address, ses_id, retransmit);
    close(socket_fd);
}

/* ===================================== TCP FUNCTIONS ========================================= */

void receive_CON_ACC_tcp(int socket_fd, uint64_t ses_id) {
    static char buffer[CONACC_LEN];
    while (true) {
        ssize_t length = readn(socket_fd, buffer, CONACC_LEN);
        if (length < 0) {
            close(socket_fd);
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // Timeout.
                close(socket_fd);
                fatal("could not receive packet");
            }
            syserr("readn");
        }
        uint64_t res_ses_id;
        uint8_t type = buffer[0];
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

void receive_RCVD_RJT_tcp(int socket_fd, uint64_t ses_id) {
    while (true) {
        static char buffer[RJT_LEN];
        ssize_t length1 = readn(socket_fd, buffer, RCVD_LEN);
        if (length1 < 0) {
            close(socket_fd);
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // Timeout.
                close(socket_fd);
                fatal("could not receive packet");
            }
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
        } else if (res_type == RJT) {
            ssize_t length2 = readn(socket_fd, buffer, RJT_LEN - RCVD_LEN);
            if (length2 < 0) {
                close(socket_fd);
                if (errno == EAGAIN || errno == EWOULDBLOCK)  // Timeout.
                    fatal("could not receive packet");
                syserr("readn");
            }
            if (length1 + length2 < RJT_LEN) {
                close(socket_fd);
                fatal("invalid packet length");
            }
            uint64_t res_packet_nr;
            memcpy(&res_packet_nr, buffer, 8);
            res_packet_nr = be64toh(res_packet_nr);
            close(socket_fd);
            fatal("packet number %ld was rejected", res_packet_nr);
        } else {
            close(socket_fd);
            fatal("invalid packet type");
        }
        break;
    }
}

void send_DATA_tcp(int socket_fd, struct sockaddr_in server_address, uint64_t ses_id, char *data,
                   uint64_t seq_len) {
    uint64_t already_sent = 0;
    uint64_t packet_nr = 0;
    while (already_sent < seq_len) {
        uint32_t bytes_nr = min((uint64_t)DATA_PACKET_SIZE, seq_len - already_sent);
        send_one_DATA_packet(socket_fd, server_address, ses_id, packet_nr, bytes_nr, data,
                             already_sent, PROT_TCP);
        already_sent += bytes_nr;
        packet_nr++;
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
    if (connect(socket_fd, (struct sockaddr *)&server_address, (socklen_t)sizeof(server_address))<0)
        syserr("cannot connect to the server");

    srand(time(NULL));
    uint64_t ses_id = get_random();

    send_CONN(socket_fd, server_address, ses_id, PROT_TCP, seq_len);

    receive_CON_ACC_tcp(socket_fd, ses_id);
    send_DATA_tcp(socket_fd, server_address, ses_id, data, seq_len);
    receive_RCVD_RJT_tcp(socket_fd, ses_id);
    close(socket_fd);
}

/* ===================================== MAIN FUNCTION ========================================= */

int main(int argc, char *argv[]) {
    if (argc != 4)
        fatal("usage: %s <protocol> <host> <port>", argv[0]);
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
    vector<char> vec_input;
    char act;
    do {
        act = getc(stdin);
        vec_input.push_back(act);
    } while (act != EOF);
    vec_input.pop_back();
    u_int64_t seq_len = vec_input.size();
    char *input = (char *)malloc(seq_len);
    if (input == NULL) syserr("malloc");

    for (uint64_t i = 0; i < seq_len; i++) input[i] = vec_input[i];

    struct sockaddr_in server_address = get_server_address(host, port);
    if (prot == PROT_UDP || prot == PROT_UDPR)
        udp_client(server_address, input, seq_len, prot);
    else
        tcp_client(server_address, input, seq_len);
    free(input);
}