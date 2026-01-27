
#pragma once

#include <poll.h>
#include <sys/un.h>

enum {
    ERROR_BUFFER_SIZE = 256
};

struct server_context {
    int argc;
    const char **argv;

    int exit_code;
    const char *exit_message;
    char error_buffer[ERROR_BUFFER_SIZE];

    struct sockaddr_un addr;

    const char *socket_path;
    int listen_fd;

    struct pollfd *pollfds;
    nfds_t pollfds_capacity;
    nfds_t num_clients;
    int *client_sockets;
};
