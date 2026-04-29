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
    UNUSED(argc);
    UNUSED(argv);

    printf("Hello, World!\n");

cleanup:
    return status;
}

/*** end of file ***/
