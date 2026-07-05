/** @file main.c
 *
 * @brief Main source
 *
 * @par
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"
#include "server.h"
#include "chat.h"
#include "main.h"

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;

    server_t * p_server = NULL;
    uint16_t   lport    = DEFAULT_LPORT;

    int opt = -1;
    while (-1 != (opt = getopt(argc, argv, "p:")))
    {
        switch (opt)
        {
            case 'p':
            {
                int const ten = 10;
                uint64_t  u64 = strtoul(optarg, NULL, ten);
                if (u64 > UINT16_MAX)
                {
                    fprintf(stderr, "Port must be [0-%hu]\n", UINT16_MAX);
                    status = STATUS_FAILURE;
                    goto cleanup;
                }
                lport = u64;
                break;
            }

            default:
            {
                fprintf(stderr, "Usage: %s [-p port]\n", argv[0]);
                status = STATUS_FAILURE;
                goto cleanup;
            }
        }
    }

    if (optind != argc)
    {
        fprintf(stderr, "Unexpected positional arguments\n");
        status = STATUS_FAILURE;
        goto cleanup;
    }

    server_t hints =
    {
        .lport         = lport,
        .p_server_init = chat_server_init,
        .p_server_free = chat_server_free,
    };

    p_server = server_create(&hints);

    server_run(p_server);

cleanup:
    server_destroy(p_server);
    p_server = NULL;

    if ((STATUS_SUCCESS != status) && (0 != errno))
    {
        fprintf(stderr, "errno: %d\n", errno);
    }

    return status;
}

/*** end of file ***/
