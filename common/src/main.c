/** @file main.c
 *
 * @brief Main source
 *
 * @par
 *
 */

#include "main.h"

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;
    UNUSED(argc);
    UNUSED(argv);

    void * p_buf = NULL;

    size_t size = 6u;

    p_buf = malloc(size);
    if (NULL == p_buf)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memset(p_buf, 2, size);

    memcpy((uint8_t *)p_buf + 1, "abc", 3u);

    fprint(stdout, p_buf, size, NULL, NULL, NULL, NULL, NULL, true);

cleanup:
    free(p_buf);
    p_buf = NULL;
    return status;
}

/*** end of file ***/
