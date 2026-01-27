
#ifndef SERVER_H
#define SERVER_H

#include <poll.h>
#include <sys/un.h>

struct server_context {
    int argc;
    char **argv;

    int exit_code;
    const char *exit_message;
    char error_buffer[256];

    struct sockaddr_un addr;

    const char *socket_path;
    int listen_fd;

    struct pollfd *pollfds;
    nfds_t pollfds_capacity;
    nfds_t num_clients;
    int *client_sockets;
};

#endif /*SERVER_H*/