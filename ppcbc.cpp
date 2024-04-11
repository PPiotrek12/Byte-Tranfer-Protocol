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
#include "udp_client_lib.h"
#include "err.h"
#include "common.h"
#include "protconst.h"

using namespace std;

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
    if (prot == PROT_UDP || prot == PROT_UDPR)
        udp_client(server_address, input, seq_len, prot);
    else 
        tcp_client(server_address, input, seq_len);
}