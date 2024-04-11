#ifndef TCP_CLIENT_LIB_H
#define TPC_CLIENT_LIB_H

void tcp_client(struct sockaddr_in server_address, char *data, uint64_t seq_len);

#endif // TCP_CLIENT_LIB_H