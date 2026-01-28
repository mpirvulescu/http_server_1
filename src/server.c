#include "../include/server.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static server_context init_context()
{
    server_context ctx = {0};

    ctx.argc = 0;
    ctx.argv = NULL;

    ctx.exit_code        = EXIT_SUCCESS;
    ctx.exit_message     = NULL;
    ctx.listen_fd        = -1;
    ctx.num_clients      = 0;
    ctx.pollfds          = NULL;
    ctx.clients          = NULL;
    ctx.pollfds_capacity = 0;

    return ctx;
}

static void parse_arguments(server_context *ctx);

static void validate_arguments(server_context *ctx);

static void print_usage(const server_context *ctx);

static void init_server_socket(const server_context *ctx)
{
}

static void event_loop(const server_context *ctx)
{
}

static void accept_client(const server_context *ctx)
{
}

static void close_client(const server_context *ctx)
{
}

static void cleanup_server(const server_context *ctx)
{
}

static void read_request(const server_context *ctx)
{
}

static void parse_http_request(const server_context *ctx)
{
}

static void validate_http_request(const server_context *ctx)
{
}

static void dispatch_method(const server_context *ctx)
{
}

static void handle_get(const server_context *ctx)
{
}

static void handle_head(const server_context *ctx)
{
}

static void handle_post(const server_context *ctx)
{
}

static void map_url_to_path(const server_context *ctx)
{
}

static void check_file(const server_context *ctx)
{
}

static void read_file(const server_context *ctx)
{
}

static void send_response_headers(const server_context *ctx)
{
}

static void send_response_body(const server_context *ctx)
{
}

static void send_error_response(const server_context *ctx)
{
}

static void set_status(const server_context *ctx)
{
}

int main(const int argc, char **argv)
{
    server_context ctx;
    ctx      = init_context();
    ctx.argc = argc;
    ctx.argv = argv;

    parse_arguments(&ctx);
    validate_arguments(&ctx);

    return EXIT_SUCCESS;
}

static void parse_arguments(server_context *ctx)
{
    int         opt;
    const char *optstring = "h:p:f";
    opterr                = 0;

    while((opt = getopt(ctx->argc, ctx->argv, optstring)) != -1)
    {
        switch(opt)
        {
            case 'p':
                ctx->user_entered_port = optarg;
                break;
            case 'f':
                ctx->root_directory = optarg;
                break;
            case 'h':
                ctx->exit_code = EXIT_SUCCESS;
                print_usage(ctx);
                break;
            case ':':
                fprintf(stderr, "Error: Option %c requires an argument.\n", optopt);
                ctx->exit_code = EXIT_FAILURE;
                print_usage(ctx);
                break;
            case '?':
                fprintf(stderr, "Error: unknown option: -%c\n", optopt);
                ctx->exit_code = EXIT_FAILURE;
                print_usage(ctx);
                break;
            default:
                ctx->exit_code = EXIT_FAILURE;
                print_usage(ctx);
                break;
        }
    }
}

static void validate_arguments(server_context *ctx)
{
    if(ctx->user_entered_port == NULL)
    {
        fprintf(stderr, "Error: Port number is required (-p <port>).\n");
        ctx->exit_code = EXIT_FAILURE;
        print_usage(ctx);
        return;
    }

    if(ctx->root_directory == NULL)
    {
        fprintf(stderr, "Error: Root Directory is required (-f <root directory).\n");
        ctx->exit_code = EXIT_FAILURE;
        print_usage(ctx);
        return;
    }

    char *endptr;
    errno = 0;
    unsigned long user_defined_port;
    user_defined_port = strtoul(ctx->user_entered_port, &endptr, 10);

    if(errno != 0 || *endptr != '\0' || user_defined_port > 65535 || user_defined_port < 0)
    {
        fprintf(stderr, "Error: Invalid port number. Must be between 0 and 65535.\n");
        ctx->exit_code = EXIT_FAILURE;
        print_usage(ctx);
        return;
    }

    struct stat st;
    if(stat(ctx->root_directory, &st) != 0)
    {
        fprintf(stderr, "Error: Cannot access directory: %s\n", ctx->root_directory);
        ctx->exit_code = EXIT_FAILURE;
        return;
    }

    if(!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Error: '%s' is not a directory.\n", ctx->root_directory);
        ctx->exit_code = EXIT_FAILURE;
        return;
    }

    if(access(ctx->root_directory, R_OK | X_OK) != 0)
    {
        fprintf(stderr, "Error: you do not have the necessary permissions for directory: %s\n", ctx->root_directory);
        ctx->exit_code = EXIT_FAILURE;
        return;
    }

    ctx->port_number = (uint16_t)user_defined_port;
}

static void print_usage(const server_context *ctx)
{
    fprintf(stderr, "Usage: %s, <Port number>\n", ctx->argv[0]);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "-h Display this help and exit\n");
}
