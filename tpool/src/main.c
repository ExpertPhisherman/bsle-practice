/** @file main.c
 *
 * @brief Main source
 *
 */

#include "main.h"

static size_t const num_threads = 4u;
static size_t const num_items   = 100u;

/*!
 * @brief Sample worker function
 *
 * @param[in] p_arg Pointer to argument
 *
 * @return void
 */
static void worker(void * p_arg);

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;
    UNUSED(argc);
    UNUSED(argv);

    tpool_t * p_tm;
    int     * p_vals;

    p_tm   = tpool_create(num_threads);
    p_vals = calloc(num_items, sizeof(*p_vals));

    for (size_t idx = 0u; idx < num_items; idx++)
    {
        p_vals[idx] = idx;
        tpool_add_work(p_tm, worker, p_vals + idx);
    }

    tpool_wait(p_tm);

    for (size_t idx = 0u; idx < num_items; idx++)
    {
        printf("%d\n", p_vals[idx]);
    }

    free(p_vals);
    p_vals = NULL;
    tpool_destroy(p_tm);
    p_tm = NULL;

    return status;
}

static void
worker (void * p_arg)
{
    int * p_val =  p_arg;
    int   old   = *p_val;

    *p_val += 1000;
    printf("tid=%p, old=%d, val=%d\n", (void *)pthread_self(), old, *p_val);

    if (*p_val % 2)
    {
        usleep(100000u);
    }

    return;
}

/*** end of file ***/
