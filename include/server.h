
#pragma once

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/un.h>

enum {
    ERROR_BUFFER_SIZE = 256,
    PORT_INPUT_BASE = 10,
    MAX_PORT_NUMBER = 65535,

    BASE_REQUEST_BUFFER_CAPACITY = 30,
    REQUEST_BUFFER_INCREASE_THRESHOLD = 20,
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

    size_t request_buffer_capacity;
    size_t request_buffer_filled;
    char *request_buffer;

    http_request request;
};

typedef struct client_state client_state;

struct server_context {
    int argc;
    char **argv;

    int exit_code;
    char *exit_message; // Dynamically allocated

    struct sockaddr_un addr;

    int listen_fd;
    const char* user_entered_port;
    uint16_t port_number;
    const char *root_directory;

    struct pollfd *pollfds;
    nfds_t pollfds_capacity;

    nfds_t num_clients;
    struct client_state* clients;
};

typedef struct server_context server_context;

