/** @file main.c
 *
 * @brief Main source
 *
 */

#include "main.h"

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;

    //pthread_create();

    (void)argc;
    (void)argv;

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    return status;
}

/*** end of file ***/
