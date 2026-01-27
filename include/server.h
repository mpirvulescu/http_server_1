
#pragma once

#include <poll.h>
#include <stdint.h>
#include <sys/un.h>

enum {
    ERROR_BUFFER_SIZE = 256
};

struct client_state {
    int client_sockets;
};

struct server_context {
    int argc;
    const char **argv;

    int exit_code;
    char *exit_message; // Dynamically allocated

    struct sockaddr_un addr;

    int listen_fd;
    uint16_t port;
    const char *root_directory;

    struct pollfd *pollfds;
    nfds_t pollfds_capacity;

    nfds_t num_clients;
    struct client_state* clients;
};

typedef struct client_state client_state;
typedef struct server_context server_context;

