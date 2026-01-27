#include "../include/server.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct server_context Context;

static void parse_arguments(Context *ctx) {};
static void validate_arguments(Context *ctx) {};
static void print_usage(Context *ctx) {};
static void init_server_socket(Context *ctx) {};
static void event_loop(Context *ctx) {};
static void accept_client(Context *ctx) {};
static void close_client(Context *ctx) {};
static void cleanup_server(Context *ctx) {};
static void read_request(Context *ctx) {};
static void parse_http_request(Context *ctx) {};
static void validate_http_request(Context *ctx) {};
static void dispatch_method(Context *ctx) {};
static void handle_get(Context *ctx) {};
static void handle_head(Context *ctx) {};
static void handle_post(Context *ctx) {};
static void map_url_to_path(Context *ctx) {};
static void check_file(Context *ctx) {};
static void read_file(Context *ctx) {};
static void send_response_headers(Context *ctx) {};
static void send_response_body(Context *ctx) {};
static void send_error_response(Context *ctx) {};
static void set_status(Context *ctx) {};

int main(const int argc, const char *argv[])
{
    Context ctx = {0};

    ctx.argc = argc;
    ctx.argv = argv;

    ctx.exit_code        = EXIT_SUCCESS;
    ctx.listen_fd        = -1;
    ctx.num_clients      = 0;
    ctx.pollfds          = NULL;
    ctx.client_sockets   = NULL;
    ctx.pollfds_capacity = 0;

    parse_arguments(&ctx);
    validate_arguments(&ctx);

    return EXIT_SUCCESS;
}
