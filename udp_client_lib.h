#ifndef UDP_CLIENT_LIB_H
#define UDP_CLIENT_LIB_H

void udp_client(struct sockaddr_in server_address, char *data, uint64_t seq_len, uint8_t prot);

#endif // UDP_CLIENT_LIB_H