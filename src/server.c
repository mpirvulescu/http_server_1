#include "../include/server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables
static volatile sig_atomic_t exit_flag = 0;

static void parse_http_request(const server_context *ctx, client_state *state);

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

__attribute__((noreturn)) static void quit(const server_context *ctx);

static void parse_arguments(server_context *ctx);

static void validate_arguments(server_context *ctx);

__attribute__((noreturn)) static void print_usage(const server_context *ctx);

static void setup_signal_handler(void);

static void signal_handler(int sig);

static int convert_address(server_context *ctx);

static void init_server_socket(server_context *ctx);

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

static void read_request(const server_context *ctx, client_state *state)
{
    const char  *request_sentinel        = "\r\n\r\n";
    const size_t request_sentinel_length = strlen(request_sentinel);

    // Allocate request buffer
    state->request_buffer = malloc(BASE_REQUEST_BUFFER_CAPACITY);
    if(state->request_buffer == NULL)
    {
        close_client(ctx, state);
        return;
    }
    state->request_buffer_capacity = BASE_REQUEST_BUFFER_CAPACITY;
    state->request_buffer_filled   = 0;

    bool isEndOfRequest;
    do
    {
        size_t remaining_buffer_space = state->request_buffer_capacity - state->request_buffer_filled;

        // Reallocate if there is not much space
        if(remaining_buffer_space < REQUEST_BUFFER_INCREASE_THRESHOLD)
        {
            const size_t new_capacity       = state->request_buffer_capacity * 2;
            void        *new_buffer_pointer = realloc(state->request_buffer, new_capacity);
            if(new_buffer_pointer == NULL)
            {
                free(state->request_buffer);
                state->request_buffer_capacity = 0;
                state->request_buffer_filled   = 0;
                close_client(ctx, state);
                return;
            }
            state->request_buffer          = new_buffer_pointer;
            state->request_buffer_capacity = new_capacity;
        }

        // Read into the buffer
        const ssize_t result = read(state->socket, state->request_buffer + state->request_buffer_filled, 1);
        switch(result)
        {
            case -1:    // ERROR
                if(errno != EINTR)
                {
                    free(state->request_buffer);
                    state->request_buffer_capacity = 0;
                    state->request_buffer_filled   = 0;
                    close_client(ctx, state);
                    return;
                }
            case 0:    // EOF
                free(state->request_buffer);
                state->request_buffer_capacity = 0;
                state->request_buffer_filled   = 0;
                close_client(ctx, state);
                return;
            default:
                state->request_buffer_filled += result;
        }

        isEndOfRequest = strncmp(state->request_buffer + state->request_buffer_filled - request_sentinel_length, request_sentinel, request_sentinel_length) == 0;
    } while(isEndOfRequest);
    state->request_buffer[state->request_buffer_filled] = '\0';
    parse_http_request(ctx, state);
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
    init_server_socket(&ctx);

    return EXIT_SUCCESS;
}

__attribute__((noreturn)) static void quit(const server_context *ctx)
{
    if(ctx->exit_message != NULL)
    {
        fputs(ctx->exit_message, stderr);
    }
    exit(ctx->exit_code);
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

// bad parse arguments I think

// static void parse_arguments(server_context *ctx)
// {
//     int         opt;
//     const char *optstring = "h:p:f:"; //lol the bug was no colon after f
//     opterr                = 0;

//     while((opt = getopt(ctx->argc, ctx->argv, optstring)) != -1)
//     {
//         switch(opt)
//         {
//             case 'p':
//                 ctx->user_entered_port = optarg;
//                 break;
//             case 'f':
//                 ctx->root_directory = optarg;
//                 break;
//             case 'h':
//                 ctx->exit_code = EXIT_SUCCESS;
//                 print_usage(ctx);
//                 break;
//             case ':':
//                 fprintf(stderr, "Error: Option %c requires an argument.\n", optopt);
//                 ctx->exit_code = EXIT_FAILURE;
//                 print_usage(ctx);
//                 break;
//             case '?':
//                 fprintf(stderr, "Error: unknown option: -%c\n", optopt);
//                 ctx->exit_code = EXIT_FAILURE;
//                 print_usage(ctx);
//                 break;
//             default:
//                 ctx->exit_code = EXIT_FAILURE;
//                 print_usage(ctx);
//                 break;
//         }
//     }
// }

// NEW GOOD WAY  I THINK
static void parse_arguments(server_context *ctx)
{
    int opt;
    // leading colon ':' tells getopt to return ':' for missing arguments
    // instead of printing its own default error message.
    const char *optstring = ":p:f:i:h";
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
            case 'i':
                ctx->ip_address = optarg;
                break;
            case 'h':
                ctx->exit_code = EXIT_SUCCESS;
                print_usage(ctx);
            case ':':
                // deals with case where user types "-p" but forgets the value
                fprintf(stderr, "Error: Option '-%c' requires an argument.\n", optopt);
                ctx->exit_code = EXIT_FAILURE;
                print_usage(ctx);
            case '?':
                // deals with unknown options (e.g. "-z")
                fprintf(stderr, "Error: Unknown option '-%c'.\n", optopt);
                ctx->exit_code = EXIT_FAILURE;
                print_usage(ctx);
            default:
                ctx->exit_code = EXIT_FAILURE;
                print_usage(ctx);
        }
    }
}

static void validate_arguments(server_context *ctx)
{
    // check the damn flags
    if(ctx->user_entered_port == NULL)
    {
        fputs("Error: Port number is required (-p <port>).\n", stderr);
        ctx->exit_code = EXIT_FAILURE;
        print_usage(ctx);
    }

    if(ctx->root_directory == NULL)
    {
        fputs("Error: Root Directory is required (-f <dir>).\n", stderr);
        ctx->exit_code = EXIT_FAILURE;
        print_usage(ctx);
    }

    // validate port
    char *endptr;
    errno                           = 0;
    unsigned long user_defined_port = strtoul(ctx->user_entered_port, &endptr, PORT_INPUT_BASE);

    if(errno != 0 || *endptr != '\0' || user_defined_port > UINT16_MAX)
    {
        fprintf(stderr, "Error: Invalid port number '%s'. Must be 0-65535.\n", ctx->user_entered_port);
        ctx->exit_code = EXIT_FAILURE;
        quit(ctx);
    }

    ctx->port_number = (uint16_t)user_defined_port;

    // validate directory
    struct stat st;
    if(stat(ctx->root_directory, &st) != 0)
    {
        perror("Error accessing root directory");
        ctx->exit_code = EXIT_FAILURE;
        quit(ctx);
    }

    if(!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Error: '%s' is not a directory.\n", ctx->root_directory);
        ctx->exit_code = EXIT_FAILURE;
        quit(ctx);
    }

    if(access(ctx->root_directory, R_OK | X_OK) != 0)
    {
        fprintf(stderr, "Error: No permissions to read/execute directory '%s'.\n", ctx->root_directory);
        ctx->exit_code = EXIT_FAILURE;
        quit(ctx);
    }

    // default to localhost if not provided, this makes sense i think
    if(ctx->ip_address == NULL)
    {
        ctx->ip_address = "127.0.0.1";
    }

    // putting convert address as helper to follow FSM more
    if(convert_address(ctx) == -1)
    {
        fprintf(stderr, "Error: '%s' is not a valid IPv4 or IPv6 address.\n", ctx->ip_address);
        ctx->exit_code = EXIT_FAILURE;
        quit(ctx);
    }
}

__attribute__((noreturn)) static void print_usage(const server_context *ctx)
{
    // updated usage
    fprintf(stderr, "Usage: %s -p <port> -f <root_directory> [-i <ip_address>] [-h]\n", ctx->argv[0]);
    fputs("\nOptions:\n", stderr);
    fputs("  -p <port>   Port number to listen on (Required)\n", stderr);
    fputs("  -f <path>   Path to document root (Required)\n", stderr);
    fputs("  -i <ip>     IP address to bind (Default: 127.0.0.1)\n", stderr);
    fputs("  -h          Display this help and exit\n", stderr);
    quit(ctx);
}

static void setup_signal_handler(void)
{
    struct sigaction sa = {0};
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif
    sa.sa_handler = signal_handler;
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
    sigaction(SIGINT, &sa, NULL);
}

static void signal_handler(int sig)
{
    exit_flag = 1;
}

static int convert_address(server_context *ctx)
{
    memset(&ctx->addr, 0, sizeof(ctx->addr));

    // IPv4
    if(inet_pton(AF_INET, ctx->ip_address, &(((struct sockaddr_in *)&ctx->addr)->sin_addr)) == 1)
    {
        ctx->addr.ss_family = AF_INET;
        return 0;
    }

    // IPv6
    if(inet_pton(AF_INET6, ctx->ip_address, &(((struct sockaddr_in6 *)&ctx->addr)->sin6_addr)) == 1)
    {
        ctx->addr.ss_family = AF_INET6;
        return 0;
    }

    return -1;
}

static void init_server_socket(server_context *ctx)
{
    // create
    int sockfd;

    /*stupid darcy build forces me to use SOCK_STREAM | SOCK_CLOEXEC but
    SHIT DOESNT WORK ON MAC BRO ITS FOR ANDROID AND LINUX PEOPLE.
    I needed to add the NOLINT shit to stop fucking my asshole with the stupid warning
    */
    sockfd = socket(ctx->addr.ss_family, SOCK_STREAM, 0);    // NOLINT(android-cloexec-socket)

    if(sockfd == -1)
    {
        fprintf(stderr, "Error: socket could not be created\n");
        ctx->exit_code = EXIT_FAILURE;
        quit(ctx);
    }

    // FIX: Set FD_CLOEXEC manually for portability
    // READ THIS: i have no idea if this shit will work for you Giorgio, i asked ai. godspeed
    if(fcntl(sockfd, F_SETFD, FD_CLOEXEC) == -1)
    {
        fprintf(stderr, "Error: fcntl failed\n");
        close(sockfd);
        ctx->exit_code = EXIT_FAILURE;
        quit(ctx);
    }

    // skip the stupid timeout phase
    int enable;
    enable = 1;

    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1)
    {
        fprintf(stderr, "Error: setsockopt failed\n");
        ctx->exit_code = EXIT_FAILURE;
        quit(ctx);
    }

    // bind
    char      addr_str[INET6_ADDRSTRLEN];
    socklen_t addr_len;
    void     *vaddr;
    in_port_t net_port;

    net_port = htons(ctx->port_number);

    if(ctx->addr.ss_family == AF_INET)
    {
        struct sockaddr_in *ipv4_addr;

        ipv4_addr           = (struct sockaddr_in *)&ctx->addr;
        addr_len            = sizeof(*ipv4_addr);
        ipv4_addr->sin_port = net_port;
        vaddr               = (void *)&(((struct sockaddr_in *)&ctx->addr)->sin_addr);
    }
    else if(ctx->addr.ss_family == AF_INET6)
    {
        struct sockaddr_in6 *ipv6_addr;

        ipv6_addr            = (struct sockaddr_in6 *)&ctx->addr;
        addr_len             = sizeof(*ipv6_addr);
        ipv6_addr->sin6_port = net_port;
        vaddr                = (void *)&(((struct sockaddr_in6 *)&ctx->addr)->sin6_addr);
    }
    else
    {
        fprintf(stderr, "Error: addr->ss_family must be AF_INET or AF_INET6, was: %d\n", ctx->addr.ss_family);
        ctx->exit_code = EXIT_FAILURE;
        print_usage(ctx);
    }

    if(inet_ntop(ctx->addr.ss_family, vaddr, addr_str, sizeof(addr_str)) == NULL)
    {
        fprintf(stderr, "Error: inet_ntop failed\n");
        ctx->exit_code = EXIT_FAILURE;
        print_usage(ctx);
    }

    printf("Binding to %s:%u\n", addr_str, ctx->port_number);

    if(bind(sockfd, (struct sockaddr *)&ctx->addr, addr_len) == -1)
    {
        fprintf(stderr, "Error: binding to the socket failed\n");
        ctx->exit_code = EXIT_FAILURE;
        print_usage(ctx);
    }

    printf("Bound to socket: %s:%u\n", addr_str, ctx->port_number);

    // listen
    if(listen(sockfd, SOMAXCONN) == -1)
    {
        fprintf(stderr, "Listening failed\n");
        close(sockfd);
        ctx->exit_code = EXIT_FAILURE;
        print_usage(ctx);
    }

    printf("Listening for incoming connections...\n");

    ctx->listen_fd = sockfd;
}
