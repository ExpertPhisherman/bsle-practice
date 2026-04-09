/** @file main.c
 *
 * @brief Main source
 *
 */

#include "../include/main.h"

static size_t const num_threads = 4u;
static size_t const num_items   = 100u;

void
worker (void * p_arg)
{
    int * p_val =  p_arg;
    int   old   = *p_val;

    *p_val += 1000;
    printf("tid=%p, old=%d, val=%d\n", (void*)pthread_self(), old, *p_val);

    if (*p_val % 2)
    {
        usleep(100000u);
    }

    return;
}

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;

    tpool_t * p_tm;
    int     * p_vals;
    size_t    index;

    p_tm   = tpool_create(num_threads);
    p_vals = calloc(num_items, sizeof(*p_vals));

    for (index = 0u; index < num_items; index++)
    {
        p_vals[index] = index;
        tpool_add_work(p_tm, worker, p_vals + index);
    }

    tpool_wait(p_tm);

    for (index = 0u; index < num_items; index++)
    {
        printf("%d\n", p_vals[index]);
    }

    free(p_vals);
    p_vals = NULL;
    tpool_destroy(p_tm);
    p_tm = NULL;

    (void)argc;
    (void)argv;

    return status;
}

/*** end of file ***/
