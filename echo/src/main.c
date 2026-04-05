/** @file main.c
 *
 * @brief Main source
 *
 */

#include "main.h"

void
handle_sigchld (int signo)
{
    int saved_errno = errno;
    (void)signo;

    while (0 < waitpid(-1, NULL, WNOHANG))
    {
        // Reap all child processes that have terminated
    }

    errno = saved_errno;

    return;
}

void
handle_sigint (int signo)
{
    (void)signo;
    g_keep_running = 0;
}

int
main (int argc, char * argv[])
{
    status_t status;

    int opt;
    bool b_verbose = false;
    uint16_t server_port = 0u;
    int backlog = DEFAULT_BACKLOG;

    while (-1 != (opt = getopt(argc, argv, "vp:b:")))
    {
        // Enforce command line integer sizes
        int64_t i64;
        uint64_t u64;

        switch (opt)
        {
            case 'v':
                b_verbose = true;
                break;

            case 'b':
                i64 = strtol(optarg, NULL, 10);
                if ((1 <= i64) && (INT_MAX >= i64))
                {
                    backlog = (int)i64;
                }
                else
                {
                    fprintf(stderr, "Backlog must be [1-%d]\n", INT_MAX);
                    status = STATUS_FAILURE;
                    goto cleanup;
                }
                break;

            case 'p':
                u64 = strtoul(optarg, NULL, 10);
                if (MAX_PORT >= u64)
                {
                    server_port = (uint16_t)u64;
                }
                else
                {
                    fprintf(stderr, "Port must be [1-65535]\n");
                    status = STATUS_FAILURE;
                    goto cleanup;
                }
                break;

            default:
                fprintf(stderr, "Usage: %s [-v] [-b backlog] -p port\n", argv[0]);
                status = STATUS_FAILURE;
                goto cleanup;
        }
    }

    if (optind != argc)
    {
        fprintf(stderr, "Unexpected positional arguments\n");
        status = STATUS_FAILURE;
        goto cleanup;
    }

    if (0u == server_port)
    {
        fprintf(stderr, "Argument -p port is required (positive integer)\n");
        fprintf(stderr, "Usage: %s [-v] [-b backlog] -p port\n", argv[0]);
        status = STATUS_FAILURE;
        goto cleanup;
    }

    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = handle_sigchld;
    sa_chld.sa_flags = SA_RESTART;
    sigemptyset(&sa_chld.sa_mask);

    if (-1 == sigaction(SIGCHLD, &sa_chld, NULL))
    {
        perror("sigaction SIGCHLD");
        status = STATUS_SIGNAL_FAILURE;
        goto cleanup;
    }

    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sa_int.sa_flags = 0;
    sigemptyset(&sa_int.sa_mask);

    if (-1 == sigaction(SIGINT, &sa_int, NULL))
    {
        perror("sigaction SIGINT");
        status = STATUS_SIGNAL_FAILURE;
        goto cleanup;
    }

    session_t session =
    {
        .server_port = server_port,
        .client_port = 0u,
        .server_sockfd = -1,
        .client_sockfd = -1,
        .backlog = backlog,
        .b_verbose = b_verbose
    };

    status = server_socket(&session);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    // NOTE: Test SLL functions before accepting client connection
    sll_t sll;
    sll_create(&sll);
    sll_append(&sll, "abcd", 5u);
    sll_append(&sll, "1234", 5u);
    sll_append(&sll, &(uint32_t){2u}, 4u);
    sll_append(&sll, &(uint32_t){3u}, 4u);
    sll_display(&sll);
    sll_destroy(&sll);

    status = client_socket(&session);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    close(session.server_sockfd);
    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "errno: %d\n", errno);
    }

    return status;
}

/*** end of file ***/
