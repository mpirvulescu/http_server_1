#include "../include/server.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static void close_client(const server_context *ctx, const client_state *state)
{
}

static void cleanup_server(const server_context *ctx)
{
}

static void read_request(const server_context *ctx, const client_state *state)
{
}

struct split_string
{
    size_t count;
    char **strings;
};

static struct split_string str_split(char *string, const char *delimiter)
{
    size_t              substring_count;
    char               *last_delimiter;
    size_t              cur_substring;
    char               *token;
    size_t              delimiter_length;
    struct split_string result;
    bool                lastIndexWasDelim = false;
    char               *temp_string;

    result.count = 0;
    if(delimiter == NULL || delimiter[0] == '\0' || string == NULL || string[0] == '\0')
    {
        result.strings = NULL;
        return result;
    }

    temp_string = strdup(string);
    token       = strtok(temp_string, delimiter);
    while(token != NULL)
    {
        result.count++;
        token = strtok(NULL, delimiter);
    }
    free(temp_string);

    if(result.count == 0)
    {
        result.strings = NULL;
        return result;
    }

    result.strings = (char **)malloc(sizeof(char *) * result.count);
    memset((void *)result.strings, 0, (sizeof(char *) * result.count));

    if(result.strings == NULL)
    {
        result.strings = NULL;
        return result;
    }

    cur_substring = 0;
    temp_string   = strdup(string);
    token         = strtok(string, delimiter);
    while(token != NULL)
    {
        if(cur_substring == result.count)
        {
            // Somehow more tokens than we expected
            printf("str_split error: more tokens than expected");
            break;
        }
        result.strings[cur_substring] = strdup(token);
        token                         = strtok(NULL, delimiter);
        cur_substring++;
    }
    free(temp_string);

    return result;
}

static void free_split_string(struct split_string split_string)
{
    if(split_string.strings == NULL)
    {
        return;
    }

    // Free all of the duped strings
    for(int i = 0; i < split_string.count; i++)
    {
        if(split_string.strings[i] == NULL)
        {
            continue;
        }
        free(split_string.strings[i]);
        split_string.strings[i] = NULL;
    }
    // Free the array itself
    free((void *)split_string.strings);
    split_string.strings = NULL;
}

static void parse_http_request(const server_context *ctx, client_state *state)
{
    struct split_string lines;
    lines = str_split(state->request_buffer, "\r\n");

    if(lines.count < 1)
    {
        free_split_string(lines);
        close_client(ctx, state);
        return;
    }

    struct split_string mainParts = str_split(lines.strings[0], " ");
    if(mainParts.count != 3)
    {
        free_split_string(lines);
        free_split_string(mainParts);
        close_client(ctx, state);
        return;
    }
    if(strcmp(mainParts.strings[2], "HTTP/1.0") != 0)
    {
        free_split_string(lines);
        free_split_string(mainParts);
        close_client(ctx, state);
        return;
    }
    state->request.method          = strdup(mainParts.strings[0]);
    state->request.path            = strdup(mainParts.strings[1]);
    state->request.protocolVersion = strdup(mainParts.strings[2]);

    for(int line = 1; line < lines.count; line++)
    {
        // lines.strings[line] // New header
    }

    free_split_string(mainParts);
    free_split_string(lines);
}

static void validate_http_request(const client_state *state)
{
}

static void dispatch_method(const client_state *state)
{
}

static void handle_get(const client_state *state)
{
}

static void handle_head(const client_state *state)
{
}

static void handle_post(const client_state *state)
{
}

static void map_url_to_path()
{
}

static void check_file()
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

// Main for testing parse_http_request
// int main()
// {
//     client_state state = {0};
//
//     // Different requests to test
//     // const char *req = "GET    /weird   HTTP/1.0\r\nhOsT:example.com\r\nUser-Agent:TestAgent/0.1\r\nAccept: */*\r\n\r\n";
//     // const char *req = "GET http://example.com/path/to/resource?x=1&y=2 HTTP/1.0\r\nUser-Agent: TestAgent/0.1\r\nAccept: text/html\r\n\r\n";
//     const char *req      = "GET /index.html HTTP/1.0\r\nHost: example.com\r\nUser-Agent: TestAgent/0.1\r\nAccept: */*\r\n\r\n";
//     state.request_buffer = strdup(req);
//
//     parse_http_request(NULL, &state);
//     free(state.request_buffer);
//
//     printf("%s\n%s\n%s\n", state.request.method, state.request.path, state.request.protocolVersion);
//
//     free(state.request.method);
//     free(state.request.path);
//     free(state.request.protocolVersion);
//
//     return EXIT_SUCCESS;
// }

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
        fprintf(stderr, "Error: Root Directory is required (-f <root directory>).\n");
        ctx->exit_code = EXIT_FAILURE;
        print_usage(ctx);
        return;
    }

    char *endptr;
    errno = 0;
    unsigned long user_defined_port;
    user_defined_port = strtoul(ctx->user_entered_port, &endptr, PORT_INPUT_BASE);

    if(errno != 0 || *endptr != '\0' || user_defined_port > MAX_PORT_NUMBER || user_defined_port < 0)
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
    fprintf(stderr, "Usage: %s -p <port number> -f <root directory>\n", ctx->argv[0]);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "-h Display this help and exit\n");
}
