/** @file main.c
 *
 * @brief Main source
 *
 */

#include "../include/main.h"

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;

    printf("Hello, World!\n");

    goto cleanup;

cleanup:
    (void)argc;
    (void)argv;

    return status;
}

/*** end of file ***/
