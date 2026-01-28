
#pragma once

#include <poll.h>
#include <sys/un.h>

enum {
    ERROR_BUFFER_SIZE = 256
};

struct http_request {
    char *method;
    char *path;
    char *protocolVersion;
    char **headers;
};

typedef struct http_request http_request;

struct client_state {
    int socket;

    int request_buffer_size;
    char *request_buffer;

    http_request request;
};

typedef struct client_state client_state;

struct server_context {
    int argc;
    const char **argv;

    int exit_code;
    char *exit_message; // Dynamically allocated

    struct sockaddr_un addr;

    const char *socket_path;
    int listen_fd;

    struct pollfd *pollfds;
    nfds_t pollfds_capacity;

    nfds_t num_clients;
    struct client_state* clients;
};

typedef struct server_context server_context;

