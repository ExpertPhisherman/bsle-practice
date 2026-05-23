/** @file main.c
 *
 * @brief Main source
 *
 * @par
 *
 */

#include "main.h"

extern uint16_t const max_port;
extern uint16_t const default_lport;
extern size_t const   creds_capacity;
extern size_t const   rooms_capacity;

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;

    int        opt;
    uint16_t   lport     = default_lport;
    bool       b_verbose = false;
    server_t * p_server  = NULL;

    server_t hints =
    {
        .lport         = lport,
        .sockfd        = -1,
        .epollfd       = -1,
        .b_verbose     = b_verbose,
        .p_tm          = NULL,
        .p_registry    = NULL,
        .p_client_run  = NULL,
        .p_client_init = NULL,
        .p_client_free = NULL,
        .p_appdata     = NULL,
    };

    while (-1 != (opt = getopt(argc, argv, "vp:")))
    {
        // Enforce command line integer sizes
        uint64_t u64;

        switch (opt)
        {
        case 'v':
            b_verbose = true;
            break;

            case 'p':
                u64 = strtoul(optarg, NULL, 10);
                if (u64 > max_port)
            {
                    fprintf(stderr, "Port must be [0-%hu]\n", max_port);
                status = STATUS_FAILURE;
                goto cleanup;
            }
                lport = u64;
            break;

        default:
                fprintf(stderr, "Usage: %s [-v] [-p port]\n", argv[0]);
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

    hints.lport         = lport;
    hints.b_verbose     = b_verbose;
    hints.p_client_run  = chat_client_run;
    hints.p_client_init = chat_client_init;
    hints.p_client_free = chat_client_free;

    p_server = chat_server_create(&hints, creds_capacity, rooms_capacity);
    if (NULL == p_server)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    server_run(p_server);

cleanup:
    chat_server_destroy(p_server);

    if ((STATUS_SUCCESS != status) && (0 != errno))
    {
        fprintf(stderr, "errno: %d\n", errno);
    }

    return status;
}

/*** end of file ***/
