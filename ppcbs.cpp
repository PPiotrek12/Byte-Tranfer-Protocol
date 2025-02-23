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

#include <string>

#include "common.h"
#include "err.h"
#include "protconst.h"

using namespace std;
const int DATA_MAX_SIZE = 64000;

/* ==================================== COMMON FUNCTIONS ======================================= */

void send_CONACC(int socket_fd, struct sockaddr_in client_address, uint64_t ses_id, uint8_t prot) {
    static char message[CONACC_LEN];
    message[0] = CONACC;
    memcpy(message + 1, &ses_id, 8);
    send_message(socket_fd, message, sizeof(message), client_address, prot);
}

void send_CONRJT(int socket_fd, struct sockaddr_in client_address, uint64_t ses_id, uint8_t prot) {
    static char message[CONRJT_LEN];
    message[0] = CONRJT;
    memcpy(message + 1, &ses_id, 8);
    send_message(socket_fd, message, sizeof(message), client_address, prot);
}

void send_RCVD(int socket_fd, struct sockaddr_in client_address, uint64_t ses_id, uint8_t prot) {
    static char message[RCVD_LEN];
    message[0] = RCVD;
    memcpy(message + 1, &ses_id, 8);
    send_message(socket_fd, message, sizeof(message), client_address, prot);
}

void send_RJT(int socket_fd, struct sockaddr_in client_address, uint64_t ses_id, uint64_t packet_nr,
              uint8_t prot) {
    static char message[RJT_LEN];
    message[0] = RJT;
    memcpy(message + 1, &ses_id, 8);
    packet_nr = htobe64(packet_nr);
    memcpy(message + 9, &packet_nr, 8);
    send_message(socket_fd, message, sizeof(message), client_address, prot);
}

void send_ACC(int socket_fd, struct sockaddr_in client_address, uint64_t ses_id, uint64_t packet_nr,
              uint8_t prot) {
    static char message[ACC_LEN];
    message[0] = ACC;
    memcpy(message + 1, &ses_id, 8);
    packet_nr = htobe64(packet_nr);
    memcpy(message + 9, &packet_nr, 8);
    send_message(socket_fd, message, sizeof(message), client_address, prot);
}

// Returns: 0-correct; 1-next client (close connection); 2-ignore packet (or also close in TCP).
int check_received_DATA_packet(int socket_fd, struct sockaddr_in res_address, uint64_t ses_id,
                               uint64_t seq_len, ssize_t read_length, uint64_t already_read,
                               uint64_t last_packet_nr, uint8_t res_type, uint64_t res_ses_id,
                               uint64_t res_packet_nr, uint32_t res_bytes_nr, uint8_t prot) {
    if (res_type == CONN) {                  // Someone is trying to interrupt.
        if (prot == PROT_UDPR && res_ses_id == ses_id) return 2;  // Ignoring CONN sent twice.
        send_CONRJT(socket_fd, res_address, res_ses_id, prot);
        return 2;
    }
    if (res_ses_id != ses_id) {  // Client authorisation.
        err("invalid session id");
        return 2;  // Ignore packet.
    }
    if (res_type != DATA) {  // Correct client send invalid packet - close connection.
        err("invalid packet type");
        return 1;  // Next client.
    }
    if (read_length < MIN_DATA_LEN) {  // Correct client send invalid packet.
        err("invalid packet length");
        send_RJT(socket_fd, res_address, res_ses_id, res_packet_nr, prot);
        return 1;  // Next client.
    }
    if (res_packet_nr != last_packet_nr + 1) {
        if (prot == PROT_UDPR && res_packet_nr < last_packet_nr + 1) return 2;  // Ignore packet.
        err("invalid packet number");
        send_RJT(socket_fd, res_address, res_ses_id, res_packet_nr, prot);
        return 1;  // Next client.
    }
    if (already_read + res_bytes_nr > seq_len) {
        err("invalid bytes number");
        send_RJT(socket_fd, res_address, res_ses_id, res_packet_nr, prot);
        return 1;  // Next client.
    }
    return 0;
}

/* ======================================= UDP FUNCTIONS ======================================= */

void receive_CONN_udp(int socket_fd, struct sockaddr_in *client_address, uint64_t *ses_id,
                      uint8_t *prot, uint64_t *seq_len) {
    static char buffer[CONN_LEN];
    while (true) {
        socklen_t address_length = (socklen_t)sizeof(client_address);
        ssize_t length = recvfrom(socket_fd, buffer, CONN_LEN, 0, (struct sockaddr *)client_address,
                                  &address_length);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)  // Timeout.
                continue;
            syserr("recvfrom");
        }
        uint8_t res_type = buffer[0];
        uint8_t res_prot = buffer[9];
        if (res_type != CONN) {
            err("invalid packet type");
            continue;  // Next client.
        }
        if (length != CONN_LEN) {
            err("invalid packet length");
            continue;  // Next client.
        }
        if (res_prot != PROT_UDP && res_prot != PROT_UDPR) {
            err("invalid protocol");
            continue;  // Next client.
        }
        break;
    }
    // Here we have received correct CONN packet.
    memcpy(ses_id, buffer + 1, 8);
    *prot = buffer[9];
    memcpy(seq_len, buffer + 10, 8);
    *seq_len = be64toh(*seq_len);
}

// Returns 1 if there is need to close connection.
int receive_one_DATA_packet_udp(int socket_fd, struct sockaddr_in *res_address, uint64_t last_packet_nr,
                            uint64_t seq_len, char *buffer, uint8_t prot, uint64_t already_read,
                            uint64_t ses_id, uint64_t *res_packet_nr, uint32_t *res_bytes_nr) {
    int retransmits = 0;
    if (prot == PROT_UDPR) retransmits = MAX_RETRANSMITS;
    while (true) {
        socklen_t address_length = (socklen_t)sizeof(res_address);
        ssize_t length = recvfrom(socket_fd, buffer, DATA_MAX_SIZE + MIN_DATA_LEN, 0,
                                  (struct sockaddr *)res_address, &address_length);
        if (length < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {  // Timeout.
                if (retransmits > 0) {
                    resend_last_message();
                    retransmits--;
                    continue;
                } else {
                    err("could not receive packet DATA");
                    return 1;  // Next client.
                }
            }
            syserr("recvfrom");
        }
        uint64_t res_ses_id;
        uint8_t res_type = buffer[0];
        memcpy(&res_ses_id, buffer + 1, 8);
        memcpy(res_packet_nr, buffer + 9, 8);
        *res_packet_nr = be64toh(*res_packet_nr);
        memcpy(res_bytes_nr, buffer + 17, 4);
        *res_bytes_nr = be32toh(*res_bytes_nr);
        int res = check_received_DATA_packet(socket_fd, *res_address, ses_id, seq_len, length,
                                             already_read, last_packet_nr, res_type, res_ses_id,
                                             *res_packet_nr, *res_bytes_nr, prot);
        if (res == 1) return 1;
        if (res == 2) continue;
        break;
    }
    return 0;
}

// Returns 1 if there is need to close connection.
int receive_DATA_udp(int socket_fd, uint64_t ses_id, uint8_t prot, uint64_t seq_len, char *data) {
    uint64_t already_read = 0, last_packet_nr = -1;
    while (already_read < seq_len) {
        // Receiving packet.
        static char buffer[DATA_MAX_SIZE + MIN_DATA_LEN];
        struct sockaddr_in res_address;
        uint64_t res_packet_nr;
        uint32_t res_bytes_nr;

        if (receive_one_DATA_packet_udp(socket_fd, &res_address, last_packet_nr, seq_len, buffer, prot,
                                    already_read, ses_id, &res_packet_nr, &res_bytes_nr))
            return 1;
        // Here we have received correct DATA packet.
        memcpy(data + already_read, buffer + MIN_DATA_LEN - 1, res_bytes_nr);
        already_read += res_bytes_nr;
        last_packet_nr = res_packet_nr;
        if (prot == PROT_UDPR) send_ACC(socket_fd, res_address, ses_id, res_packet_nr, prot);
    }
    return 0;
}

void udp_server(struct sockaddr_in server_address) {
    int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) syserr("socket");
    struct timeval timeout;
    timeout.tv_sec = MAX_WAIT;
    timeout.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)))
        syserr("setsockopt");
    if (bind(socket_fd, (struct sockaddr *)&server_address, (socklen_t)sizeof(server_address)) < 0)
        syserr("bind");

    while (true) {
        uint8_t prot;
        uint64_t seq_len, ses_id;
        struct sockaddr_in client_address;
        receive_CONN_udp(socket_fd, &client_address, &ses_id, &prot, &seq_len);
        send_CONACC(socket_fd, client_address, ses_id, prot);

        char *data = (char *)malloc(seq_len);
        if (data == NULL) syserr("malloc");
        if (receive_DATA_udp(socket_fd, ses_id, prot, seq_len, data)) {
            free(data);
            continue;  // Next client.
        }
        while (seq_len > 0) {
            ssize_t written = write(STDOUT_FILENO, data, seq_len);
            if (written < 0) syserr("write");
            seq_len -= written;
        }
        fflush(stdout);
        free(data);

        send_RCVD(socket_fd, client_address, ses_id, prot);
    }
    close(socket_fd);
}

/* ===================================== TCP FUNCTIONS ========================================= */

const int QUEUE_LENGTH = 10000;

// Returns 1 if there is need to close connection.
int receive_CONN_tcp(int client_fd, uint64_t *ses_id, uint8_t *prot, uint64_t *seq_len) {
    static char buffer[CONN_LEN];
    ssize_t length = readn(client_fd, buffer, CONN_LEN);
    if (length < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {  // Timeout.
            err("could not receive packet CONN");
            return 1;  // Next client.
        }
        syserr("readn");
    }
    uint8_t res_type = buffer[0];
    uint8_t res_prot = buffer[9];
    if (length != CONN_LEN) {
        err("invalid packet length");
        return 1;  // Next client.
    }
    if (res_type != CONN) {
        err("invalid packet type");
        return 1;  // Next client.
    }
    if (res_prot != PROT_TCP) {
        err("invalid protocol%d", *prot);
        return 1;  // Next client.
    }
    // Here we have received correct CONN packet.
    memcpy(ses_id, buffer + 1, 8);
    *prot = buffer[9];
    memcpy(seq_len, buffer + 10, 8);
    *seq_len = be64toh(*seq_len);
    return 0;
}

// Returns 1 if there is need to close connection.
int receive_DATA_tcp(int client_fd, uint64_t ses_id, uint64_t seq_len, char *data) {
    uint64_t already_read = 0, last_packet_nr = -1;
    while (already_read < seq_len) {
        static char buffer[DATA_MAX_SIZE + MIN_DATA_LEN];
        uint64_t res_ses_id, res_packet_nr;
        uint32_t res_bytes_nr;
        uint8_t res_type;

        ssize_t length1 = readn(client_fd, buffer, MIN_DATA_LEN - 1);  // Reading packet header.
        if (length1 < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {  // Timeout.
                err("could not receive packet DATA");
                return 1;  // Next client.
            }
            syserr("readn");
        }
        res_type = buffer[0];
        memcpy(&res_ses_id, buffer + 1, 8);
        memcpy(&res_packet_nr, buffer + 9, 8);
        res_packet_nr = be64toh(res_packet_nr);
        memcpy(&res_bytes_nr, buffer + 17, 4);
        res_bytes_nr = be32toh(res_bytes_nr);
        ssize_t length2 = readn(client_fd, buffer, res_bytes_nr);  // Reading data.
        if (length2 < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {  // Timeout.
                err("could not receive packet DATA");
                return 1;  // Next client.
            }
            syserr("readn");
        }
        struct sockaddr_in null;
        int res = check_received_DATA_packet(client_fd, null, ses_id, seq_len, length1 + length2,
                                             already_read, last_packet_nr, res_type, res_ses_id,
                                             res_packet_nr, res_bytes_nr, PROT_TCP);
        if (res == 1 || res == 2) return 1;
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
    if (socket_fd < 0) syserr("socket");
    if (bind(socket_fd, (struct sockaddr *)&server_address, (socklen_t)sizeof(server_address)) < 0)
        syserr("bind");
    if (listen(socket_fd, QUEUE_LENGTH) < 0) syserr("listen");
    while (true) {
        struct sockaddr_in client_address;
        socklen_t address_length = (socklen_t)sizeof(client_address);
        int client_fd = accept(socket_fd, (struct sockaddr *)&client_address, &address_length);         
        if (client_fd < 0) syserr("accept");
        struct timeval timeout;
        timeout.tv_sec = MAX_WAIT;
        timeout.tv_usec = 0;
        if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)))
            syserr("setsockopt");

        struct linger linger_opt;
        linger_opt.l_onoff = 1;
        linger_opt.l_linger = 0;
        if (setsockopt(client_fd, SOL_SOCKET, SO_LINGER, (void *)&linger_opt, sizeof(linger_opt)))
            syserr("setsockopt");

        uint8_t prot;
        uint64_t seq_len, ses_id;
        if (receive_CONN_tcp(client_fd, &ses_id, &prot, &seq_len)) {
            close(client_fd);
            continue;
        }
        send_CONACC(client_fd, client_address, ses_id, PROT_TCP);

        char *data = (char *)malloc(seq_len);
        if (data == NULL) syserr("malloc");
        if (receive_DATA_tcp(client_fd, ses_id, seq_len, data)) {
            close(client_fd);
            free(data);
            continue;
        }
        while (seq_len > 0) {
            ssize_t written = write(STDOUT_FILENO, data, seq_len);
            if (written < 0) syserr("write");
            seq_len -= written;
        }
        fflush(stdout);
        send_RCVD(client_fd, client_address, ses_id, PROT_TCP);
    }
    close(socket_fd);
}

/* ===================================== MAIN FUNCTION ========================================= */

int main(int argc, char *argv[]) {
    if (argc != 3)
        fatal("usage: %s <protocol> <port>", argv[0]);
    string protocol = argv[1];
    uint16_t port = read_port(argv[2]);
    uint8_t prot = 0;
    if (protocol == "udp")
        prot = PROT_UDP;
    else if (protocol == "tcp")
        prot = PROT_TCP;
    else
        fatal("Invalid protocol");
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;                 // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);  // Listening on all interfaces.
    server_address.sin_port = htons(port);

    if (prot == PROT_UDP)
        udp_server(server_address);
    else
        tcp_server(server_address);
    return 0;
}