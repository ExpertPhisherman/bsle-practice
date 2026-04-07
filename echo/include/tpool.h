/** @file tpool.h
 *
 * @brief Thread pool header
 *
 */

#ifndef TPOOL_H
#define TPOOL_H

#include "common.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef void (*thread_func_t)(void * p_arg);

typedef struct tpool_work
{
    thread_func_t       p_func; // Function pointer
    void              * p_arg;  // Pointer to argument
    struct tpool_work * p_next; // Pointer to next work in the queue
} tpool_work_t;

typedef struct tpool
{
    tpool_work_t    * p_work_first; // Used to push and pop work objects
    tpool_work_t    * p_work_last;  // Used to push and pop work objects
    pthread_mutex_t   work_mutex;   // Used for all locking
    pthread_cond_t    work_cond;    // Signal when there is work to be processed
    pthread_cond_t    working_cond; // Signal when no threads are processing
    size_t            working_cnt;  // Number of threads actively processing work
    size_t            thread_cnt;   // Number of threads alive
    bool              stop;         // Stop threads
} tpool_t;

/*!
 * @brief Create thread pool
 *
 * @param[in] num Number of threads in pool
 *
 * @return Pointer to thread pool
 */
tpool_t * tpool_create(size_t num);

/*!
 * @brief Destroy thread pool
 *
 * @param[in] p_tm Pointer to thread pool
 *
 * @return void
 */
void tpool_destroy(tpool_t * p_tm);

/*!
 * @brief Add work to the queue for processing
 *
 * @param[in] p_tm   Pointer to thread pool
 * @param[in] p_func Function pointer
 * @param[in] p_arg  Pointer to argument
 *
 * @return Boolean
 */
bool tpool_add_work(tpool_t * p_tm, thread_func_t p_func, void * p_arg);

/*!
 * @brief Block until all work has been completed
 *
 * @param[in] p_tm Pointer to thread pool
 *
 * @return void
 */
void tpool_wait(tpool_t * p_tm);

#endif /* TPOOL_H */

/*** end of file ***/
