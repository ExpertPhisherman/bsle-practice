/** @file safe.h
 *
 * @brief Thread-safe types header
 *
 * @par
 *
 */

#ifndef SAFE_H
#define SAFE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "common.h"
#include "ht.h"
#include "sll.h"

typedef struct safe_ht
{
    ht_t            * p_ht; // Pointer to hash table
    pthread_mutex_t   lock; // Mutex lock for read/write control
} safe_ht_t;

typedef struct safe_sll
{
    sll_t           * p_sll; // Pointer to SLL
    pthread_mutex_t   lock;  // Mutex lock for read/write control
} safe_sll_t;

/*!
 * @brief Create thread-safe hash table
 *
 * @param[in] capacity Current number of buckets
 *
 * @return Pointer to thread-safe hash table
 */
safe_ht_t * safe_ht_create(size_t capacity);

/*!
 * @brief Destroy thread-safe hash table
 *
 * @param[in] p_safe_ht Pointer to thread-safe hash table
 *
 * @return Status of operation
 */
status_t safe_ht_destroy(safe_ht_t * p_safe_ht);

/*!
 * @brief Create thread-safe SLL
 *
 * @param[in] void
 *
 * @return Pointer to thread-safe SLL
 */
safe_sll_t * safe_sll_create(void);

/*!
 * @brief Destroy thread-safe SLL
 *
 * @param[in] p_safe_sll Pointer to thread-safe SLL
 *
 * @return Status of operation
 */
status_t safe_sll_destroy(safe_sll_t * p_safe_sll);

#endif /* SAFE_H */

/*** end of file ***/
