/** @file sll.c
 *
 * @brief Singly linked list source
 *
 * @par
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "sll.h"

/*!
 * @brief Destroy node
 *
 * @param[in] p_sll  Pointer to SLL
 * @param[in] p_node Pointer to node
 *
 * @return Status of operation
 */
static status_t node_destroy(sll_t * p_sll, node_t * p_node);

/*!
 * @brief Compare node data
 *
 * @param[in] p_sll   Pointer to SLL
 * @param[in] p_data1 Pointer to first data
 * @param[in] p_data2 Pointer to second data
 * @param[in] size    Size of data in bytes
 *
 * @return Difference between first and second data
 */
static int node_compare(
    sll_t      * p_sll,
    void const * p_data1,
    void const * p_data2,
    size_t       size
);

sll_t *
sll_create (void)
{
    status_t status = STATUS_SUCCESS;

    sll_t * p_sll = calloc(1u, sizeof(*p_sll));
    if (NULL == p_sll)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_sll->p_head         = NULL;
    p_sll->len            = 0u;
    p_sll->p_display_data = NULL;
    p_sll->p_compare_data = NULL;
    p_sll->p_destroy_data = NULL;

cleanup:
    if (STATUS_SUCCESS != status)
    {
        sll_destroy(p_sll);
        p_sll = NULL;
    }
    return p_sll;
}

status_t
sll_destroy (sll_t * p_sll)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_sll)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    node_t * p_curr = p_sll->p_head;
    node_t * p_next = NULL;
    while (NULL != p_curr)
    {
        p_next = p_curr->p_next;
        node_destroy(p_sll, p_curr);
        p_curr = p_next;
    }

    p_sll->p_head         = NULL;
    p_sll->len            = 0u;
    p_sll->p_display_data = NULL;
    p_sll->p_compare_data = NULL;
    p_sll->p_destroy_data = NULL;

cleanup:
    free(p_sll);
    p_sll = NULL;
    return status;
}

status_t
sll_display (sll_t * p_sll, char const * p_sep)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_sll)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (NULL == p_sll->p_head)
    {
        status = STATUS_EMPTY;
        goto cleanup;
    }

    node_t * p_curr = p_sll->p_head;
    while (NULL != p_curr)
    {
        if (NULL != p_sll->p_display_data)
        {
            (p_sll->p_display_data)(*(void **)(p_curr->p_data));
        }
        else
        {
            display_printable(p_curr->p_data, p_curr->size, ", ", "\n");
        }

        if (NULL != p_curr->p_next)
        {
            printf("%s", p_sep);
        }

        p_curr = p_curr->p_next;
    }

cleanup:
    return status;
}

node_t *
sll_get (sll_t * p_sll, void const * p_data, size_t size)
{
    node_t * p_node = NULL;

    if ((NULL == p_sll) || (NULL == p_data))
    {
        goto cleanup;
    }

    node_t * p_curr = p_sll->p_head;
    while (NULL != p_curr)
    {
        // Compare node data to passed in data
        if (0 == node_compare(p_sll, p_curr->p_data, p_data, size))
        {
            p_node = p_curr;
            goto cleanup;
        }

        p_curr = p_curr->p_next;
    }

cleanup:
    return p_node;
}

status_t
sll_insert (
    sll_t      * p_sll,
    void const * p_data,
    size_t       size,
    size_t       idx
)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_sll) || (NULL == p_data))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // NOTE: Allow idx == p_sll->len for insert at tail of SLL
    if (idx > p_sll->len)
    {
        status = STATUS_OUT_OF_BOUNDS;
        goto cleanup;
    }

    node_t * p_prev = NULL;
    node_t * p_curr = p_sll->p_head;
    while (0u < idx)
    {
        p_prev = p_curr;
        p_curr = p_curr->p_next;
        idx--;
    }

    node_t * p_node = calloc(1u, sizeof(*p_node));
    if (NULL == p_node)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_node->p_data = calloc(1u, size);
    if (NULL == p_node->p_data)
    {
        free(p_node);
        p_node = NULL;

        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memcpy(p_node->p_data, p_data, size);
    p_node->size = size;

    // Previous node links to inserted node
    if (NULL == p_prev)
    {
        p_sll->p_head = p_node;
    }
    else
    {
        p_prev->p_next = p_node;
    }

    // Inserted node links to current node
    p_node->p_next = p_curr;

    (p_sll->len)++;

cleanup:
    return status;
}

status_t
sll_append (sll_t * p_sll, void const * p_data, size_t size)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_sll) || (NULL == p_data))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    status = sll_insert(p_sll, p_data, size, p_sll->len);

cleanup:
    return status;
}

status_t
sll_remove (sll_t * p_sll, void const * p_data, size_t size)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_sll) || (NULL == p_data))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    node_t * p_prev = NULL;
    node_t * p_curr = p_sll->p_head;

    while (NULL != p_curr)
    {
        // Compare node data to passed in data
        if (0 == node_compare(p_sll, p_curr->p_data, p_data, size))
        {
            // Link skips node where data was found
            if (NULL == p_prev)
            {
                p_sll->p_head = p_curr->p_next;
            }
            else
            {
                p_prev->p_next = p_curr->p_next;
            }

            node_destroy(p_sll, p_curr);

            (p_sll->len)--;

            // NOTE: Data found in SLL
            goto cleanup;
        }

        p_prev = p_curr;
        p_curr = p_curr->p_next;
    }

    // NOTE: Data not found in SLL
    status = STATUS_NOT_EXISTS;

cleanup:
    return status;
}

static status_t
node_destroy (sll_t * p_sll, node_t * p_node)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_sll) || (NULL == p_node))
    {
        goto cleanup;
    }

    if (NULL != p_sll->p_destroy_data)
    {
        (p_sll->p_destroy_data)(*(void **)(p_node->p_data));
    }

    free(p_node->p_data);
    p_node->p_data = NULL;
    free(p_node);
    p_node = NULL;

cleanup:
    return status;
}

static int
node_compare (
    sll_t      * p_sll,
    void const * p_data1,
    void const * p_data2,
    size_t       size
)
{
    int result = 0;

    if ((NULL == p_data1) && (NULL == p_data2))
    {
        goto cleanup;
    }

    if (NULL == p_data1)
    {
        result = -1;
        goto cleanup;
    }

    if (NULL == p_data2)
    {
        result = 1;
        goto cleanup;
    }

    if (NULL == p_sll)
    {
        result = -1;
        goto cleanup;
    }

    if (NULL != p_sll->p_compare_data)
    {
        result = (p_sll->p_compare_data)(
            *(void **)p_data1,
            *(void **)p_data2,
            size
        );
        goto cleanup;
    }

    result = memcmp(p_data1, p_data2, size);

cleanup:
    return result;
}

/*** end of file ***/
