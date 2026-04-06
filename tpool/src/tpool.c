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

/*!
 * @brief Destroy work object
 *
 * @param[in] p_work Pointer to work object
 *
 * @return void
 */
static void
tpool_work_destroy (tpool_work_t * p_work)
{
    free(p_work);
    p_work = NULL;
}

/*!
 * @brief Pull work from the queue to be processed
 *
 * @param[in] p_tm Pointer to thread pool
 *
 * @return Pointer to work object
 */
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

    if (NULL == p_work->p_next)
    {
        p_tm->p_work_first = NULL;
        p_tm->p_work_last  = NULL;
    }
    else
    {
        p_tm->p_work_first = p_work->p_next;
    }

cleanup:
    return p_work;
}

/*!
 * @brief Wait for work and process it
 *
 * @param[in] p_arg Pointer to argument
 *
 * @return NULL pointer
 */
static void *
tpool_worker (void * p_arg)
{
    tpool_t      * p_tm = p_arg;
    tpool_work_t * p_work;

    // Keep thread running, and as long as it doesn't exit, it can be used
    while (1)
    {
        // Lock the mutex so nothing else manipulates the thread pool's members
        pthread_mutex_lock(&(p_tm->work_mutex));

        /*
        Check if there is any work available for processing and still running.
        Wait in a conditional until signaled and check again.
        Automatically unlock the mutex so others can acquire the lock.
        When signaled, the conditional automatically relocks the mutex.
        While loop instead of if statement to handle spurious wakeups.
        */
        while ((NULL == p_tm->p_work_first) && (!(p_tm->stop)))
        {
            pthread_cond_wait(&(p_tm->work_cond), &(p_tm->work_mutex));
        }

        /*
        Check if the pool has requested that all threads stop running and exit.
        Keep locked to modify some things outside the run loop.
        Unlock before the thread exists.
        Stop check is here to stop before pulling any work objects.
        */
        if (p_tm->stop)
        {
            break;
        }

        /*
        When signaled, pull work from the queue.
        Increment working_cnt so the thread pool knows a thread is processing.
        Unlock so other threads can pull and process work.
        Work processing happens in parallel.
        Doesn't make sense to hold the lock while work is processing.
        Lock synchronizes pulling work from the queue.
        */
        p_work = tpool_work_get(p_tm);
        (p_tm->working_cnt)++;
        pthread_mutex_unlock(&(p_tm->work_mutex));

        /*
        If there was work, process and destroy the work object.
        Possible that there was no work, so nothing needs to be done.
        */
        if (NULL != p_work)
        {
            p_work->p_func(p_work->p_arg);
            tpool_work_destroy(p_work);
        }

        /*
        After work is processed, lock mutex and decrement working_cnt.
        If there are no threads working and no items in the queue,
        a signal will be sent to inform the wait function to wake up.

        Since only one thread can hold the lock, only the last thread to
        finish its work can decrement working_cnt to zero.
        This way there aren't any threads working.
        */
        pthread_mutex_lock(&(p_tm->work_mutex));
        (p_tm->working_cnt)--;
        if ((!(p_tm->stop)) && (0u == p_tm->working_cnt) && (NULL == p_tm->p_work_first))
        {
            pthread_cond_signal(&(p_tm->working_cond));
        }
        pthread_mutex_unlock(&(p_tm->work_mutex));
    }

    /*
    Decrement thread_cnt because this thread is stopping.

    Signal tpool_wait that a thread has exited.
    tpool_destroy was called, and it's waiting for tpool_wait to exit.
    Once all threads exit, tpool_wait will return,
    allowing tpool_destroy to finish.

    Unlock the mutex last because everything is protected by it.
    Guaranteed tpool_wait won't wake up before the thread finishes its cleanup.
    Signal only runs after mutex is unlocked.
    */
    (p_tm->thread_cnt)--;
    pthread_cond_signal(&(p_tm->working_cond));
    pthread_mutex_unlock(&(p_tm->work_mutex));

    return NULL;
}

tpool_t *
tpool_create (size_t num)
{
    tpool_t   * p_tm;
    pthread_t   thread;
    size_t      index;

    if (0u == num)
    {
        num = 2u;
    }

    p_tm             = calloc(1u, sizeof(*p_tm));
    p_tm->thread_cnt = num;

    pthread_mutex_init(&(p_tm->work_mutex), NULL);
    pthread_cond_init(&(p_tm->work_cond), NULL);
    pthread_cond_init(&(p_tm->working_cond), NULL);

    p_tm->p_work_first = NULL;
    p_tm->p_work_last  = NULL;

    /*
    Requested number of threads are started and
    tpool_worker is specified as the thread function.
    Threads are detached so they will cleanup on exit.
    No need to store thread IDs because they will never be accessed directly.
    If force exit was implemented instead of waiting,
    then would need to track the IDs.
    */
    for (index = 0u; index < num; index++)
    {
        pthread_create(&thread, NULL, tpool_worker, p_tm);
        pthread_detach(thread);
    }

    return p_tm;
}

void
tpool_destroy (tpool_t * p_tm)
{
    tpool_work_t * p_work;
    tpool_work_t * p_work2;

    if (NULL == p_tm)
    {
        goto cleanup;
    }

    /*
    Throw away all pending work,
    but the caller really should have dealt with this situation,
    since they're the one that called destroy.
    Typically, destroy will only be called when all work is done and
    nothing is processing.
    However, it's possible someone is trying to force processing to stop,
    in which case there will be work queued and needing to get cleaned up.

    This only empties the queue, and since it's in the work_mutex,
    there is full access to the queue.
    This won't interfere with anything currently processing because
    the threads have pulled off the work they're working on.
    Any threads trying to pull new work will have
    finished clearing out any pending work.
    */
    pthread_mutex_lock(&(p_tm->work_mutex));
    p_work = p_tm->p_work_first;
    while (NULL != p_work)
    {
        p_work2 = p_work->p_next;
        tpool_work_destroy(p_work);
        p_work = p_work2;
    }
    p_tm->p_work_first = NULL;

    // Once queue is cleaned up, tell the threads to stop
    p_tm->stop = true;
    pthread_cond_broadcast(&(p_tm->work_cond));
    pthread_mutex_unlock(&(p_tm->work_mutex));

    /*
    Some threads may have already been running and are currently processing,
    so wait for them to finish.
    This is where a force exit parameter could be implemented,
    which would kill all threads instead of waiting on them to finish,
    but this is a bad idea.
    For example, if writing to a database, it could be corrupted.
    */
    tpool_wait(p_tm);

    // Once all outstanding processing finishes, destroy thread pool object.
    pthread_mutex_destroy(&(p_tm->work_mutex));
    pthread_cond_destroy(&(p_tm->work_cond));
    pthread_cond_destroy(&(p_tm->working_cond));

    free(p_tm);
    p_tm = NULL;

cleanup:
    return;
}

bool
tpool_add_work (tpool_t * p_tm, thread_func_t p_func, void * p_arg)
{
    bool b_added = false;

    tpool_work_t * p_work;

    if (NULL == p_tm)
    {
        goto cleanup;
    }

    // Create work object
    p_work = tpool_work_create(p_func, p_arg);
    if (NULL == p_work)
    {
        goto cleanup;
    }

    // Lock mutex and add object to linked list
    pthread_mutex_lock(&(p_tm->work_mutex));
    if (p_tm->p_work_first == NULL)
    {
        p_tm->p_work_first = p_work;
        p_tm->p_work_last  = p_tm->p_work_first;
    }
    else
    {
        p_tm->p_work_last->p_next = p_work;
        p_tm->p_work_last         = p_work;
    }

    pthread_cond_broadcast(&(p_tm->work_cond));
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

    // Lock mutex
    pthread_mutex_lock(&(p_tm->work_mutex));

    // While loop instead of if statement to handle spurious wakeups
    while (1)
    {
        /*
        Wait if there are any threads processing, still work to do,
        or if the threads are stopping and not all have exited.
        */
        if ((NULL != p_tm->p_work_first) || ((!(p_tm->stop)) && (0u != p_tm->working_cnt)) || ((p_tm->stop) && (0u != p_tm->thread_cnt)))
        {
            pthread_cond_wait(&(p_tm->working_cond), &(p_tm->work_mutex));
        }
        else
        {
            break;
        }
    }
    pthread_mutex_unlock(&(p_tm->work_mutex));

cleanup:
    // Only return when there is no work and nothing processing
    return;
}

/*** end of file ***/
