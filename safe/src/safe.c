/** @file module.c
 *
 * @brief Thread-safe types source
 *
 * @par
 *
 */

#include "safe.h"

safe_ht_t *
safe_ht_create (size_t capacity)
{
    status_t status = STATUS_SUCCESS;

    safe_ht_t * p_safe_ht = NULL;

    p_safe_ht = malloc(sizeof(*p_safe_ht));
    if (NULL == p_safe_ht)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_safe_ht->p_ht = NULL;

    if (0 != pthread_mutex_init(&(p_safe_ht->lock), NULL))
    {
        perror("pthread_mutex_init");
        status = STATUS_MUTEX_FAILURE;
        goto cleanup;
    }

    p_safe_ht->p_ht = ht_create(capacity);
    if (NULL == p_safe_ht->p_ht)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

cleanup:
    if (STATUS_SUCCESS != status)
    {
        safe_ht_destroy(p_safe_ht);
        p_safe_ht = NULL;
    }

    return p_safe_ht;
}

status_t
safe_ht_destroy (safe_ht_t * p_safe_ht)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_safe_ht)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    ht_destroy(p_safe_ht->p_ht);
    p_safe_ht->p_ht = NULL;

    pthread_mutex_destroy(&(p_safe_ht->lock));

cleanup:
    free(p_safe_ht);
    p_safe_ht = NULL;
    return status;
}

safe_sll_t *
safe_sll_create (void)
{
    status_t status = STATUS_SUCCESS;

    safe_sll_t * p_safe_sll = NULL;

    p_safe_sll = malloc(sizeof(*p_safe_sll));
    if (NULL == p_safe_sll)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_safe_sll->p_sll = NULL;

    if (0 != pthread_mutex_init(&(p_safe_sll->lock), NULL))
    {
        perror("pthread_mutex_init");
        status = STATUS_MUTEX_FAILURE;
        goto cleanup;
    }

    p_safe_sll->p_sll = sll_create();
    if (NULL == p_safe_sll->p_sll)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

cleanup:
    if (STATUS_SUCCESS != status)
    {
        safe_sll_destroy(p_safe_sll);
        p_safe_sll = NULL;
    }
    return p_safe_sll;
}

status_t
safe_sll_destroy (safe_sll_t * p_safe_sll)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_safe_sll)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sll_destroy(p_safe_sll->p_sll);
    p_safe_sll->p_sll = NULL;

    pthread_mutex_destroy(&(p_safe_sll->lock));

cleanup:
    free(p_safe_sll);
    p_safe_sll = NULL;
    return status;
}

/*** end of file ***/
