/** @file main.c
 *
 * @brief Main source
 *
 */

#include "../include/main.h"

uint16_t const default_lport = 4444u;
uint32_t const max_payload_size = 4096u;
uint32_t const max_clients = 10u;
uint32_t const worker_threads = 8u;

void
handle_sigint (int signo)
{
    UNUSED(signo);
    g_keep_running = 0;
}

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;

    int opt;
    uint16_t lport = default_lport;
    int backlog = default_backlog;
    bool b_verbose = false;

    session_t hints =
    {
        .lport = lport,
        .rport = 0u,
        .sockfd = -1,
        .backlog = backlog,
        .b_verbose = b_verbose,
        .p_tm = NULL,
        .p_registry = NULL,
    };

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
                if (max_port >= u64)
                {
                    lport = (uint16_t)u64;
                }
                else
                {
                    fprintf(stderr, "Port must be [1-%hu]\n", max_port);
                    status = STATUS_FAILURE;
                    goto cleanup;
                }
                break;

            default:
                fprintf(stderr, "Usage: %s [-v] [-b backlog] [-p port]\n", argv[0]);
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

    hints.lport = lport;
    hints.backlog = backlog;
    hints.b_verbose = b_verbose;

    session_t * p_server_session = server_session_create(&hints);
    if (NULL == p_server_session)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Accept loop
    while (g_keep_running)
    {
        session_t * p_client_session = client_session_create(p_server_session);
        if (NULL == p_client_session)
        {
            continue;
        }

        if (!tpool_add_work(p_server_session->p_tm, handle_client_wrapper, p_client_session))
        {
            fprintf(stderr, "tpool_add_work failed\n");
        }

        // NOTE: Current function no longer has ownership
        p_client_session = NULL;
    }

    if (p_server_session->b_verbose)
    {
        printf("\nGraceful shutdown on server (sockfd %d)\n", p_server_session->sockfd);
    }

cleanup:
    server_session_destroy(p_server_session);

    if ((STATUS_SUCCESS != status) && (0 != errno))
    {
        fprintf(stderr, "errno: %d\n", errno);
    }

    return status;
}

/*** end of file ***/