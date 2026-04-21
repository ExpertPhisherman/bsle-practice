/** @file main.c
 *
 * @brief Main source
 *
 * @par
 *
 */

#include "../include/main.h"

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;

    int opt;
    uint16_t lport = default_lport;
    int backlog = default_backlog;
    bool b_verbose = false;

    server_t hints =
    {
        .lport = lport,
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

    hints.lport = lport;
    hints.backlog = backlog;
    hints.b_verbose = b_verbose;
    load_app(&hints);

    server_t * p_server = server_create(&hints);
    if (NULL == p_server)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Accept loop
    while (g_keep_running)
    {
        client_t * p_client = client_create(p_server);
        if (NULL == p_client)
        {
            continue;
        }

        // TODO: Make into function session_create
        session_t * p_session = NULL;
        {
            p_session = malloc(sizeof(*p_session));
            if (NULL == p_session)
            {
                fprintf(stderr, "malloc failed\n");
                status = STATUS_NULL_ARG;
                goto cleanup;
            }

            *p_session = (session_t)
            {
                .p_server = p_server,
                .p_client = p_client,
            };
        }

        if (!tpool_add_work(p_server->p_tm, handle_session_wrapper, p_session))
        {
            fprintf(stderr, "tpool_add_work failed\n");
        }

        // NOTE: Current function no longer has ownership
        p_client = NULL;
    }

    if (p_server->b_verbose)
    {
        printf("\nGraceful shutdown on server (sockfd %d)\n", p_server->sockfd);
    }

cleanup:
    server_destroy(p_server);

    if ((STATUS_SUCCESS != status) && (0 != errno))
    {
        fprintf(stderr, "errno: %d\n", errno);
    }

    return status;
}

/*** end of file ***/