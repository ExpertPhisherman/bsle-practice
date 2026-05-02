/** @file tpool.c
 *
 * @brief Thread pool source
 *
 */

#include "tpool.h"

/*!
 * @brief Create work object
 *
 * @param[in] p_func Function pointer
 * @param[in] p_arg  Pointer to argument
 *
 * @return Pointer to work object
 */
static tpool_work_t * tpool_work_create(thread_func_t p_func, void * p_arg);

/*!
 * @brief Destroy work object
 *
 * @param[in] p_work Pointer to work object
 *
 * @return void
 */
static void tpool_work_destroy(tpool_work_t * p_work);

/*!
 * @brief Pull work from the queue to be processed
 *
 * @param[in] p_tm Pointer to thread pool
 *
 * @return Pointer to work object
 */
static tpool_work_t * tpool_work_get(tpool_t * p_tm);

/*!
 * @brief Wait for work and process it
 *
 * @param[in] p_arg Pointer to argument
 *
 * @return NULL pointer
 */
static void * tpool_worker(void * p_arg);

tpool_t *
tpool_create (size_t num)
{
    tpool_t * p_tm = NULL;

    if (0u == num)
    {
        num = 2u;
    }

    p_tm = calloc(1u, sizeof(*p_tm));
    if (NULL == p_tm)
    {
        goto cleanup;
    }

    p_tm->p_threads = calloc(num, sizeof(*(p_tm->p_threads)));
    if (NULL == p_tm->p_threads)
    {
        free(p_tm);
        p_tm = NULL;
        goto cleanup;
    }

    p_tm->p_work_first = NULL;
    p_tm->p_work_last  = NULL;
    p_tm->working_cnt  = 0u;
    p_tm->thread_cnt   = num;
    p_tm->stop         = false;

    if (0 != pthread_mutex_init(&(p_tm->work_mutex), NULL))
    {
        free(p_tm->p_threads);
        p_tm->p_threads = NULL;
        free(p_tm);
        p_tm = NULL;
        goto cleanup;
    }

    if (0 != pthread_cond_init(&(p_tm->work_cond), NULL))
    {
        pthread_mutex_destroy(&(p_tm->work_mutex));
        free(p_tm->p_threads);
        p_tm->p_threads = NULL;
        free(p_tm);
        p_tm = NULL;
        goto cleanup;
    }

    if (0 != pthread_cond_init(&(p_tm->working_cond), NULL))
    {
        pthread_cond_destroy(&(p_tm->work_cond));
        pthread_mutex_destroy(&(p_tm->work_mutex));
        free(p_tm->p_threads);
        p_tm->p_threads = NULL;
        free(p_tm);
        p_tm = NULL;
        goto cleanup;
    }

    for (size_t idx = 0u; idx < num; idx++)
    {
        if (0 != pthread_create(&((p_tm->p_threads)[idx]), NULL, tpool_worker, p_tm))
        {
            // Stop and join any threads that were already created
            pthread_mutex_lock(&(p_tm->work_mutex));
            p_tm->stop = true;
            pthread_cond_broadcast(&(p_tm->work_cond));
            pthread_mutex_unlock(&(p_tm->work_mutex));

            for (size_t jdx = 0u; jdx < idx; jdx++)
            {
                pthread_join((p_tm->p_threads)[jdx], NULL);
            }

            pthread_cond_destroy(&(p_tm->working_cond));
            pthread_cond_destroy(&(p_tm->work_cond));
            pthread_mutex_destroy(&(p_tm->work_mutex));

            free(p_tm->p_threads);
            p_tm->p_threads = NULL;
            free(p_tm);
            p_tm = NULL;
            goto cleanup;
        }
    }

cleanup:
    return p_tm;
}

void
tpool_destroy (tpool_t * p_tm)
{
    tpool_work_t * p_work  = NULL;
    tpool_work_t * p_work2 = NULL;

    if (NULL == p_tm)
    {
        goto cleanup;
    }

    // Prevent new work from being added and discard anything still queued
    pthread_mutex_lock(&(p_tm->work_mutex));

    p_tm->stop = true;

    p_work = p_tm->p_work_first;
    while (NULL != p_work)
    {
        p_work2 = p_work->p_next;
        tpool_work_destroy(p_work);
        p_work = p_work2;
    }

    p_tm->p_work_first = NULL;
    p_tm->p_work_last  = NULL;

    // Once queue is cleaned up, tell the threads to stop
    pthread_cond_broadcast(&(p_tm->work_cond));
    pthread_cond_broadcast(&(p_tm->working_cond));
    pthread_mutex_unlock(&(p_tm->work_mutex));

    // Join all worker threads
    for (size_t idx = 0u; idx < p_tm->thread_cnt; idx++)
    {
        pthread_join((p_tm->p_threads)[idx], NULL);
    }

    pthread_mutex_destroy(&(p_tm->work_mutex));
    pthread_cond_destroy(&(p_tm->work_cond));
    pthread_cond_destroy(&(p_tm->working_cond));

    free(p_tm->p_threads);
    p_tm->p_threads = NULL;
    free(p_tm);
    p_tm = NULL;

cleanup:
    return;
}

bool
tpool_add_work (tpool_t * p_tm, thread_func_t p_func, void * p_arg)
{
    bool b_added          = false;
    tpool_work_t * p_work = NULL;

    if ((NULL == p_tm) || (NULL == p_func))
    {
        goto cleanup;
    }

    // Create work object
    p_work = tpool_work_create(p_func, p_arg);
    if (NULL == p_work)
    {
        goto cleanup;
    }

    pthread_mutex_lock(&(p_tm->work_mutex));

    if (p_tm->stop)
    {
        pthread_mutex_unlock(&(p_tm->work_mutex));
        tpool_work_destroy(p_work);
        goto cleanup;
    }

    if (NULL == p_tm->p_work_first)
    {
        p_tm->p_work_first = p_work;
        p_tm->p_work_last  = p_work;
    }
    else
    {
        p_tm->p_work_last->p_next = p_work;
        p_tm->p_work_last         = p_work;
    }

    pthread_cond_signal(&(p_tm->work_cond));
    pthread_mutex_unlock(&(p_tm->work_mutex));

    b_added = true;

cleanup:
    return b_added;
}

void
tpool_wait (tpool_t * p_tm)
{
    if (NULL == p_tm)
    {
        goto cleanup;
    }

    pthread_mutex_lock(&(p_tm->work_mutex));

    // Wait until there is no queued work and no thread actively processing work
    while ((NULL != p_tm->p_work_first) || (0u != p_tm->working_cnt))
    {
        pthread_cond_wait(&(p_tm->working_cond), &(p_tm->work_mutex));
    }

    pthread_mutex_unlock(&(p_tm->work_mutex));

cleanup:
    // Only return when there is no work and nothing processing
    return;
}

static tpool_work_t *
tpool_work_create (thread_func_t p_func, void * p_arg)
{
    tpool_work_t * p_work = NULL;

    if (NULL == p_func)
    {
        goto cleanup;
    }

    p_work = malloc(sizeof(*p_work));
    if (NULL == p_work)
    {
        goto cleanup;
    }

    p_work->p_func = p_func;
    p_work->p_arg  = p_arg;
    p_work->p_next = NULL;

cleanup:
    return p_work;
}

static void
tpool_work_destroy (tpool_work_t * p_work)
{
    free(p_work);
    p_work = NULL;
}

static tpool_work_t *
tpool_work_get (tpool_t * p_tm)
{
    tpool_work_t * p_work = NULL;

    if (NULL == p_tm)
    {
        goto cleanup;
    }

    p_work = p_tm->p_work_first;
    if (NULL == p_work)
    {
        goto cleanup;
    }

    p_tm->p_work_first = p_work->p_next;
    if (NULL == p_tm->p_work_first)
    {
        p_tm->p_work_last = NULL;
    }

    p_work->p_next = NULL;

cleanup:
    return p_work;
}

static void *
tpool_worker (void * p_arg)
{
    tpool_t      * p_tm   = p_arg;
    tpool_work_t * p_work = NULL;

    if (NULL == p_tm)
    {
        goto cleanup;
    }

    while (1)
    {
        pthread_mutex_lock(&(p_tm->work_mutex));

        /*
        Wait until there is work to do or the pool is stopping.
        While loop instead of if statement to handle spurious wakeups.
        */
        while ((NULL == p_tm->p_work_first) && (!(p_tm->stop)))
        {
            pthread_cond_wait(&(p_tm->work_cond), &(p_tm->work_mutex));
        }

        // Exit when stop is set
        if (p_tm->stop)
        {
            pthread_mutex_unlock(&(p_tm->work_mutex));
            break;
        }

        p_work = tpool_work_get(p_tm);
        if (NULL != p_work)
        {
            (p_tm->working_cnt)++;
        }

        pthread_mutex_unlock(&(p_tm->work_mutex));

        if (NULL != p_work)
        {
            p_work->p_func(p_work->p_arg);
            tpool_work_destroy(p_work);
            p_work = NULL;
        }

        pthread_mutex_lock(&(p_tm->work_mutex));

        if (0u < p_tm->working_cnt)
        {
            (p_tm->working_cnt)--;
        }

        // Signal waiters when there is no queued work and no thread is working
        if ((NULL == p_tm->p_work_first) && (0u == p_tm->working_cnt))
        {
            pthread_cond_broadcast(&(p_tm->working_cond));
        }

        pthread_mutex_unlock(&(p_tm->work_mutex));
    }

cleanup:
    return NULL;
}

/*** end of file ***/
