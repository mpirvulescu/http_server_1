#include "../include/server.h"
#include <stdio.h>
#include <stdlib.h>

static void parse_arguments(struct server_context *ctx);
static void validate_arguments(struct server_context *ctx);
static void print_usage(struct server_context *ctx);
static void init_server_socket(struct server_context *ctx);
static void event_loop(struct server_context *ctx);
static void accept_client(struct server_context *ctx);
static void close_client(struct server_context *ctx);
static void cleanup_server(struct server_context *ctx);
static void read_request(struct server_context *ctx);
static void parse_http_request(struct server_context *ctx);
static void validate_http_request(struct server_context *ctx);
static void dispatch_method(struct server_context *ctx);
static void handle_get(struct server_context *ctx);
static void handle_head(struct server_context *ctx);
static void handle_post(struct server_context *ctx);
static void map_url_to_path(struct server_context *ctx);
static void check_file(struct server_context *ctx);
static void read_file(struct server_context *ctx);
static void send_response_headers(struct server_context *ctx);
static void send_response_body(struct server_context *ctx);
static void send_error_response(struct server_context *ctx);
static void set_status(struct server_context *ctx);

int main(void)
{
    struct server_context ctx = {0};

    ctx.argc = argc;
    ctx.argv = argv;

    ctx.exit_code = EXIT_SUCCESS;
    ctx.listen_fd = -1;
    ctx.num_clients = 0;
    ctx.pollfds = NULL;
    ctx.client_sockets = NULL;
    ctx.pollfds_capacity = 0;

    parse_arguments(&ctx);
    validate_arguments(&ctx);

    return EXIT_SUCCESS;
}
