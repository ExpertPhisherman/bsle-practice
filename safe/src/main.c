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

    safe_data_t * p_safe_data = NULL;
    void        * p_buf       = NULL;
    size_t        size        = 8u;

    p_buf = malloc(size);
    if (NULL == p_buf)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memset(p_buf, 0, size);
    memcpy((uint8_t *)p_buf, "abc", 3u);

    ht_t * p_ht = ht_create(17u);
    p_safe_data = safe_data_create(p_ht);

    pthread_mutex_lock(&(p_safe_data->lock));
    ht_set(p_ht, (void *)"obama", 5u, (void *)"pyramid1", 8u);
    ht_set(p_ht, (void *)"biden", 5u, (void *)"minecraft", 9u);
    ht_display(p_safe_data->p_data, ", ");
    pthread_mutex_unlock(&(p_safe_data->lock));

    safe_data_destroy(p_safe_data);
    ht_destroy(p_ht);

cleanup:
    free(p_buf);
    p_buf = NULL;
    return status;
}

/*** end of file ***/
