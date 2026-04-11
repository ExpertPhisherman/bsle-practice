/** @file sll.c
 *
 * @brief Singly linked list source
 *
 */

#include "sll.h"

status_t
sll_create (sll_t * p_sll)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_sll)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_sll->p_head = NULL;
    p_sll->len = 0u;

    goto cleanup;

cleanup:
    return status;
}

status_t
sll_display (sll_t * p_sll)
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

    // Traverse nodes
    node_t * p_curr = p_sll->p_head;
    while (NULL != p_curr)
    {
        display_bytes(p_curr->p_data, p_curr->size, " ");

        // Update current node
        p_curr = p_curr->p_next;

        if (NULL != p_curr)
        {
            printf(" -> ");
        }
    }
    printf("\n");

    goto cleanup;

cleanup:
    return status;
}

bool
sll_in (sll_t * p_sll, void * p_data, size_t size)
{
    bool b_data_in = false;

    if (NULL == p_sll)
    {
        goto cleanup;
    }

    // Traverse nodes until data is found
    node_t * p_curr = p_sll->p_head;
    while (NULL != p_curr)
    {
        // Compare node data to passed in data
        if ((p_curr->size == size) && (0 == memcmp(p_curr->p_data, p_data, size)))
        {
            b_data_in = true;
            goto cleanup;
        }

        // Update current node
        p_curr = p_curr->p_next;
    }

    goto cleanup;

cleanup:
    return b_data_in;
}

status_t
sll_insert (sll_t * p_sll, void * p_data, size_t size, size_t idx)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_sll)
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
        // Update previous node
        p_prev = p_curr;

        // Update current node
        p_curr = p_curr->p_next;

        // Decrement index
        idx--;
    }

    // Allocate node
    node_t * p_target = malloc(sizeof(*p_target));
    if (NULL == p_target)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Allocate node data
    p_target->p_data = malloc(size);
    if (NULL == p_target->p_data)
    {
        free(p_target);
        p_target = NULL;

        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Write node data
    memcpy(p_target->p_data, p_data, size);

    // Write node data size
    p_target->size = size;

    // Previous node links to inserted node
    if (NULL == p_prev)
    {
        p_sll->p_head = p_target;
    }
    else
    {
        p_prev->p_next = p_target;
    }

    // Inserted node links to current node
    p_target->p_next = p_curr;

    // Increment SLL length
    (p_sll->len)++;

    goto cleanup;

cleanup:
    if (STATUS_ALLOC_FAILURE == status)
    {
        sll_destroy(p_sll);
    }

    return status;
}

status_t
sll_append (sll_t * p_sll, void * p_data, size_t size)
{
    return sll_insert(p_sll, p_data, size, p_sll->len);
}

status_t
sll_remove (sll_t * p_sll, void * p_data, size_t size)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_sll)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Traverse nodes until data is found
    node_t * p_prev = NULL;
    node_t * p_curr = p_sll->p_head;

    while (NULL != p_curr)
    {
        // Compare node data to passed in data
        if ((p_curr->size == size) && (0 == memcmp(p_curr->p_data, p_data, size)))
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

            // Free node where data was found
            free(p_curr->p_data);
            p_curr->p_data = NULL;

            free(p_curr);
            p_curr = NULL;

            // Decrement SLL length
            (p_sll->len)--;

            // NOTE: Data found in SLL
            goto cleanup;
        }

        // Update previous node
        p_prev = p_curr;

        // Update current node
        p_curr = p_curr->p_next;
    }

    // NOTE: Data not found in SLL
    status = STATUS_NOT_EXISTS;
    goto cleanup;

cleanup:
    return status;
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

    // Traverse nodes
    node_t * p_curr = p_sll->p_head;
    while (NULL != p_curr)
    {
        // Free current node
        node_t * p_next = p_curr->p_next;

        free(p_curr->p_data);
        p_curr->p_data = NULL;

        free(p_curr);
        p_curr = NULL;

        // Update current node
        p_curr = p_next;
    }

    goto cleanup;

cleanup:
    p_sll->p_head = NULL;
    p_sll->len = 0u;

    return status;
}

/*** end of file ***/
