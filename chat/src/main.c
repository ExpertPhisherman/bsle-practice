/** @file main.c
 *
 * @brief Main source
 *
 */

#include "../include/main.h"

int16_t const default_lport = 4444u;
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
    registry_t registry;

    session_t session =
    {
        .lport = lport,
        .rport = 0u,
        .server_sockfd = -1,
        .client_sockfd = -1,
        .backlog = backlog,
        .b_verbose = b_verbose,
        .p_tm = NULL,
        .p_registry = NULL
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

    session.lport = lport;
    session.backlog = backlog;
    session.b_verbose = b_verbose;

    status = server_socket(&session);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    registry.sockfds = malloc(max_clients * sizeof(*(registry.sockfds)));
    if (NULL == registry.sockfds)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    registry.count = 0u;
    memset(registry.sockfds, -1, max_clients * sizeof(*(registry.sockfds)));
    if (0 != pthread_mutex_init(&(registry.lock), NULL))
    {
        perror("pthread_mutex_init");
        status = STATUS_MUTEX_FAILURE;
        goto cleanup;
    }
    session.p_registry = &registry;

    session.p_tm = tpool_create(worker_threads);
    if (NULL == session.p_tm)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    status = client_socket(&session);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

cleanup:
    // Unblock all workers blocked in recv()
    if (NULL != session.p_registry)
    {
        pthread_mutex_lock(&(registry.lock));
        for (size_t index = 0u; index < max_clients; index++)
        {
            if (-1 != (registry.sockfds)[index])
            {
                if (-1 == shutdown((registry.sockfds)[index], SHUT_RDWR))
                {
                    perror("shutdown");
                }
            }
        }
        pthread_mutex_unlock(&(registry.lock));
    }

    // Wait for queued work to finish and join all threads
    if (NULL != session.p_tm)
    {
        tpool_wait(session.p_tm);
        tpool_destroy(session.p_tm);
        session.p_tm = NULL;
    }

    if (NULL != session.p_registry)
    {
        pthread_mutex_destroy(&(registry.lock));
        session.p_registry = NULL;
    }

    if (-1 != session.server_sockfd)
    {
        if (-1 == close(session.server_sockfd))
        {
            perror("close");
            status = STATUS_SOCKET_FAILURE;
        }
    }

    if ((STATUS_SUCCESS != status) && (0 != errno))
    {
        fprintf(stderr, "errno: %d\n", errno);
    }

    free(registry.sockfds);
    registry.sockfds = NULL;

    return status;
}

/*** end of file ***/
