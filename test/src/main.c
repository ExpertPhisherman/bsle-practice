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

    int * p_num = malloc(sizeof(*p_num));
    nfree((void const **)&p_num);

    goto cleanup;

cleanup:
    (void)argc;
    (void)argv;

    return status;
}

/*** end of file ***/
