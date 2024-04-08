#ifndef MIM_COMMON_H
#define MIM_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

const uint8_t CONN = 1;
const uint8_t CONACC = 2;
const uint8_t CONRJT = 3;
const uint8_t DATA = 4;
const uint8_t ACC = 5;
const uint8_t REJ = 6;
const uint8_t RCVD = 7;

const uint8_t PROT_TCP = 1;
const uint8_t PROT_UDP = 2;
const uint8_t PROT_UDPR = 3;

uint16_t read_port(char const *string);
struct sockaddr_in get_server_address(char const *host, uint16_t port);
void send_message(int socket_fd, char *message, ssize_t message_length, struct sockaddr_in server_address);

#endif
