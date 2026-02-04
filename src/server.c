#define _POSIX_C_SOURCE 200809L    // NOLINT

#include "../include/server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef NI_MAXHOST
    #define NI_MAXHOST 1025
#endif
#ifndef NI_MAXSERV
    #define NI_MAXSERV 32
#endif

enum
{
    INITIAL_POLLFDS_CAPACITY = 11
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables
static volatile sig_atomic_t exit_flag = 0;

static int parse_http_request(const server_context *ctx, client_state *state);

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

static void init_poll_fds(server_context *ctx);

static void event_loop(server_context *ctx);

static void accept_client(server_context *ctx);

static void close_client(server_context *ctx, const client_state *state);

static void cleanup_server(const server_context *ctx);

static void read_request(server_context *ctx, client_state *state)
{
    const char  *request_sentinel        = "\r\n\r\n";
    const size_t request_sentinel_length = strlen(request_sentinel);

    // markp: added freeing existing buffer if it exists to prevent leak/overwrite
    // needed to set to null after free so closeing client doesn't double-free if error
    if(state->request_buffer)
    {
        free(state->request_buffer);
        state->request_buffer = NULL;
    }

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
                    // i made close_client, fixing build
                    //  free(state->request_buffer);
                    //  state->request_buffer_capacity = 0;
                    //  state->request_buffer_filled   = 0;
                    close_client(ctx, state);
                    return;
                }
                break;    // mark p added
            case 0:       // EOF
                // mark p made close_client, fixing build
                //  free(state->request_buffer);
                //  state->request_buffer_capacity = 0;
                //  state->request_buffer_filled   = 0;
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
    ssize_t count;
    char  **strings;
};

static int get_tokens_in_str(const char *string, const char *delimiter)
{
    char *temp_string;
    char *token;
    char *save_ptr;

    int count = 0;

    temp_string = strdup(string);
    if(temp_string == NULL)
    {
        perror("Memory allocation failed when getting tokens in string\n");
        return -1;
    }
    token = strtok_r(temp_string, delimiter, &save_ptr);
    while(token != NULL)
    {
        count++;
        token = strtok_r(NULL, delimiter, &save_ptr);
    }
    free(temp_string);
    return count;
}

static void free_split_string(struct split_string *split_string)
{
    if(split_string->strings == NULL)
    {
        return;
    }

    // Free all of the duped strings
    for(int i = 0; i < split_string->count; i++)
    {
        if(split_string->strings[i] == NULL)
        {
            continue;
        }
        free(split_string->strings[i]);
        split_string->strings[i] = NULL;
    }
    // Free the array itself
    free((void *)split_string->strings);
    split_string->strings = NULL;
}

// Returns with result.strings == NULL to signal failure
static struct split_string str_split(const char *string, const char *delimiter)
{
    char               *token;
    struct split_string result = {0};
    char               *temp_string;
    char               *token_dup;
    int                 tokens_count;
    char               *save_ptr;

    if(delimiter == NULL || delimiter[0] == '\0' || string == NULL || string[0] == '\0')
    {
        result.strings = NULL;
        return result;
    }

    tokens_count = get_tokens_in_str(string, delimiter);
    if(tokens_count <= 0)
    {
        result.strings = NULL;
        return result;
    }

    result.count   = tokens_count;
    result.strings = (char **)malloc(sizeof(char *) * result.count);
    if(result.strings == NULL)
    {
        return result;
    }
    memset((void *)result.strings, 0, (sizeof(char *) * result.count));

    temp_string = strdup(string);
    if(temp_string == NULL)
    {
        perror("Memory allocation failed when getting tokens in string\n");
        free((void *)result.strings);
        result.strings = NULL;
        return result;
    }
    token = strtok_r(temp_string, delimiter, &save_ptr);
    for(size_t i = 0; i < result.count; i++)
    {
        if(token == NULL)
        {
            // Somehow not enough tokens
            fputs("str_split error: less tokens than expected\n", stderr);
            free(temp_string);
            free_split_string(&result);
            result.strings = NULL;
            return result;
        }
        token_dup = strdup(token);
        if(token_dup == NULL)
        {
            perror("Memory allocation failed when splitting strings\n");
            free(temp_string);
            free_split_string(&result);
            result.strings = NULL;
            return result;
        }
        result.strings[i] = token_dup;

        token = strtok_r(NULL, delimiter, &save_ptr);
    }
    free(temp_string);

    return result;
}

static int parse_http_request(const server_context *ctx, client_state *state)
{
    struct split_string lines;
    lines = str_split(state->request_buffer, "\r\n");

    if(lines.count < 1 || lines.strings == NULL)
    {
        free_split_string(&lines);
        return -1;
    }

    struct split_string mainParts = str_split(lines.strings[0], " ");
    if(mainParts.count != 3 || mainParts.strings == NULL)
    {
        free_split_string(&lines);
        free_split_string(&mainParts);
        return -1;
    }

    state->request.method          = strdup(mainParts.strings[0]);
    state->request.path            = strdup(mainParts.strings[1]);
    state->request.protocolVersion = strdup(mainParts.strings[2]);

    for(int line = 1; line < lines.count; line++)
    {
        // lines.strings[line] // New header
    }

    free_split_string(&mainParts);
    free_split_string(&lines);
    return 0;
}

static int validate_http_request(const client_state *state)
{
    if(strcmp(state->request.protocolVersion, "HTTP/1.0") != 0)
    {
        return -1;
    }
    if(strcmp(state->request.method, "GET") != 0 || strcmp(state->request.method, "HEAD") != 0 || strcmp(state->request.method, "POST") != 0)
    {
        return -1;
    }
    return 0;
}

static void dispatch_method(const client_state *state)
{
}

static void map_url_to_path(const server_context *ctx, client_state *state)
{
    size_t root_directory_length;
    size_t request_path_length;
    size_t combined_length;
    char  *combined_path;
    char  *real_path;
    bool   path_is_valid;

    state->file_path      = NULL;
    root_directory_length = strlen(ctx->root_directory);
    request_path_length   = strlen(state->request.path);
    combined_length       = root_directory_length + request_path_length;

    combined_path = malloc((sizeof(char) * combined_length) + 2);
    if(combined_path == NULL)
    {
        return;
    }

    strncpy(combined_path, ctx->root_directory, root_directory_length);
    combined_path[root_directory_length] = '/';
    strncpy(combined_path + root_directory_length + 1, ctx->root_directory, request_path_length);

    real_path = realpath(combined_path, NULL);
    free(combined_path);
    if(real_path == NULL)
    {
        return;
    }

    path_is_valid = strncmp(ctx->root_directory, real_path, root_directory_length) == 0;

    if(path_is_valid)
    {
        state->file_path = real_path;
    }
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

static void handle_get(const client_state *state)
{
}

static void handle_head(const client_state *state)
{
}

static void handle_post(const client_state *state)
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
    init_poll_fds(&ctx);
    event_loop(&ctx);

    cleanup_server(&ctx);

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
    int   opt;
    char *real_root_directory;
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
                real_root_directory = realpath(optarg, NULL);
                if(real_root_directory == NULL)
                {
                    fprintf(stderr, "Error: Failed getting real path of root directory \"%s\".\n", optarg);
                    ctx->exit_code = EXIT_FAILURE;
                    quit(ctx);
                }
                ctx->root_directory = real_root_directory;
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

static void init_poll_fds(struct server_context *ctx)
{
    // space for 1 listener and 10 clients to avoid realloc
    ctx->pollfds_capacity = INITIAL_POLLFDS_CAPACITY;

    // allocating client state array first
    ctx->clients = malloc(sizeof(client_state) * ctx->pollfds_capacity);
    if(ctx->clients == NULL)
    {
        perror("Error: client malloc failed");
        ctx->exit_code = EXIT_FAILURE;
        print_usage(ctx);
    }

    // initialize clients, avoiding garbage values
    for(nfds_t i = 0; i < ctx->pollfds_capacity; i++)
    {
        ctx->clients[i].socket = -1;
    }

    // allocating pollfd array
    ctx->pollfds = malloc(sizeof(struct pollfd) * (ctx->pollfds_capacity + 1));
    if(ctx->pollfds == NULL)
    {
        perror("Error: pollfds malloc failed");
        free(ctx->clients);
        ctx->exit_code = EXIT_FAILURE;
        print_usage(ctx);
    }

    // setting up listener which is at index 0
    ctx->pollfds[0].fd      = ctx->listen_fd;
    ctx->pollfds[0].events  = POLLIN;
    ctx->pollfds[0].revents = 0;

    for(nfds_t i = 1; i <= ctx->pollfds_capacity; i++)
    {
        ctx->pollfds[i].fd      = -1;
        ctx->pollfds[i].events  = 0;
        ctx->pollfds[i].revents = 0;
    }

    ctx->num_clients = 0;
}

static void accept_client(server_context *ctx)
{
    struct sockaddr_storage client_addr;
    socklen_t               addr_len = sizeof(client_addr);
    int                     client_fd;

    char client_host[NI_MAXHOST];
    char client_service[NI_MAXSERV];

    errno = 0;
    // accepting the client
    client_fd = accept(ctx->listen_fd, (struct sockaddr *)&client_addr, &addr_len);

    if(client_fd == -1)
    {
        if(errno == EINTR)
        {
            perror("Accept failed");
        }
        return;
    }

    // getting name info of the connection
    if(getnameinfo((struct sockaddr *)&client_addr, addr_len, client_host, NI_MAXHOST, client_service, NI_MAXSERV, 0) == 0)
    {
        printf("Accepted a new connection from %s:%s\n", client_host, client_service);
    }
    else
    {
        printf("unable to get client information\n");
    }

    // resize arrays if full
    // if(ctx->num_clients >= ctx->pollfds_capacity)
    // {
    //     size_t new_capacity = ctx->pollfds_capacity * 2;
    //
    //     // again, we need the new cap + 1 for listener
    //     struct pollfd *new_poll    = realloc(ctx->pollfds, sizeof(struct pollfd) * (new_capacity + 1));
    //     client_state  *new_clients = realloc(ctx->clients, sizeof(struct client_state) * new_capacity);
    //
    //     if(!new_poll || !new_clients)
    //     {
    //         perror("Error: realloc of new pollfds or new clients failed");
    //         // question for giorgio. how should we deal with freeing here
    //         if(new_poll)
    //         {
    //             free(new_poll);
    //         }
    //
    //         if(new_clients)
    //         {
    //             free(new_clients);
    //         }
    //         close(client_fd);
    //         return;
    //     }
    //
    //     ctx->pollfds          = new_poll;
    //     ctx->clients          = new_clients;
    //     ctx->pollfds_capacity = new_capacity;
    //
    //     // initializing new slots that we just added
    //     for(nfds_t i = ctx->num_clients + 1; i <= new_capacity; i++)
    //     {
    //         ctx->pollfds[i].fd      = -1;
    //         ctx->pollfds[i].events  = 0;
    //         ctx->pollfds[i].revents = 0;
    //     }
    // }
    if(ctx->num_clients >= ctx->pollfds_capacity)
    {
        size_t new_capacity = (ctx->num_clients + 1) * 2;

        // trying  to expand pollfds
        struct pollfd *new_poll = realloc(ctx->pollfds, sizeof(struct pollfd) * (new_capacity + 1));
        if(!new_poll)
        {
            perror("Error: realloc pollfds failed");
            // old ctx->pollfds is still valid, just reject the current client
            close(client_fd);
            return;
        }
        // success
        ctx->pollfds = new_poll;

        // trying to expand clients
        client_state *new_clients = realloc(ctx->clients, sizeof(client_state) * new_capacity);
        if(!new_clients)
        {
            perror("Error: realloc clients failed");
            // Old ctx->clients is still valid.
            // ctx->pollfds is larger now, but that is safe (unused space).
            close(client_fd);
            return;
        }
        ctx->clients = new_clients;

        // Initialize new slots
        for(nfds_t i = ctx->pollfds_capacity + 1; i <= new_capacity; i++)
        {
            ctx->pollfds[i].fd      = -1;
            ctx->pollfds[i].events  = 0;
            ctx->pollfds[i].revents = 0;
        }
        ctx->pollfds_capacity = new_capacity;
    }

    // store the clients in the pollfds array
    nfds_t poll_index   = ctx->num_clients + 1;
    nfds_t client_index = ctx->num_clients;

    ctx->pollfds[poll_index].fd      = client_fd;
    ctx->pollfds[poll_index].events  = POLLIN;
    ctx->pollfds[poll_index].revents = 0;

    memset(&ctx->clients[client_index], 0, sizeof(client_state));
    ctx->clients[client_index].socket = client_fd;

    ctx->num_clients++;
}

static void event_loop(server_context *ctx)
{
    while(!exit_flag)
    {
        int activity = poll(ctx->pollfds, ctx->num_clients + 1, -1);

        if(activity < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            perror("Error: poll failed");
            ctx->exit_code = EXIT_FAILURE;
            return;
        }

        // check listener to see if we need to accept a new client
        if(ctx->pollfds[0].revents & POLLIN)
        {
            accept_client(ctx);
        }

        // need to check clients cuz read_request might call close_client
        // need to go backwards cuz if we close a client, array shrinks.
        for(nfds_t i = ctx->num_clients; i > 0; i--)
        {
            nfds_t client_index = i - 1;
            nfds_t poll_index   = client_index + 1;

            if(ctx->pollfds[poll_index].revents & POLLIN)
            {
                read_request(ctx, &ctx->clients[client_index]);
            }
        }
    }
}

static void close_client(server_context *ctx, const client_state *state)
{
    if(state->socket != -1)
    {
        close(state->socket);
    }

    if(state->request_buffer)
    {
        free(state->request_buffer);
    }

    if(state->file_path)
    {
        free(state->file_path);
    }

    if(state->request.method)
    {
        free(state->request.method);
    }

    if(state->request.path)
    {
        free(state->request.path);
    }

    if(state->request.protocolVersion)
    {
        free(state->request.protocolVersion);
    }

    // address of specific client - address of start of array = index
    nfds_t client_index = state - ctx->clients;

    // safety check
    if((size_t)client_index >= ctx->num_clients)
    {
        return;
    }

    // remove from arrays by shifting everything down, need to fill gap left by client closing
    size_t items_to_move = ctx->num_clients - client_index - 1;

    if(items_to_move > 0)
    {
        // shifting clients array
        memmove(&ctx->clients[client_index], &ctx->clients[client_index + 1], sizeof(client_state) * items_to_move);

        // need to shift pollfds array too, but indices here are + 1 relative to clients because of listener
        memmove(&ctx->pollfds[client_index + 1], &ctx->pollfds[client_index + 2], sizeof(struct pollfd) * items_to_move);
    }

    ctx->num_clients--;

    printf("Safely removed client connection\n");
}

static void cleanup_server(const server_context *ctx)
{
    // Free the main arrays
    if(ctx->pollfds)
    {
        free(ctx->pollfds);
    }

    if(ctx->clients)
    {
        // Free any remaining client buffers
        for(nfds_t i = 0; i < ctx->num_clients; i++)
        {
            if(ctx->clients[i].request_buffer)
            {
                free(ctx->clients[i].request_buffer);
            }
            if(ctx->clients[i].request.method)
            {
                free(ctx->clients[i].request.method);
            }
            if(ctx->clients[i].request.path)
            {
                free(ctx->clients[i].request.path);
            }
            if(ctx->clients[i].request.protocolVersion)
            {
                free(ctx->clients[i].request.protocolVersion);
            }
            if(ctx->clients[i].file_path)
            {
                free(ctx->clients[i].file_path);
            }
        }
        free(ctx->clients);
    }

    if(ctx->listen_fd != -1)
    {
        close(ctx->listen_fd);
    }
}