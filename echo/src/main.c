/** @file main.c
 *
 * @brief Main source
 *
 * @par
 *
 */

#include "../include/main.h"

extern uint16_t const max_port;
extern _Atomic sig_atomic_t g_keep_running;
extern uint16_t const default_lport;
extern int const default_backlog;

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;

    int opt;
    uint16_t lport = default_lport;
    int backlog = default_backlog;
    bool b_verbose = false;
    server_t * p_server = NULL;

    server_t hints =
    {
        .lport = lport,
        .sockfd = -1,
        .backlog = backlog,
        .b_verbose = b_verbose,
        .p_tm = NULL,
        .p_registry = NULL,
        .p_client_run = NULL,
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
    echo_load_app(&hints);

    p_server = server_create(&hints);
    if (NULL == p_server)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    server_run(p_server);

cleanup:
    server_destroy(p_server);

    if ((STATUS_SUCCESS != status) && (0 != errno))
    {
        fprintf(stderr, "errno: %d\n", errno);
    }

    return status;
}

/*** end of file ***/
