/** @file bst.c
 *
 * @brief Binary search tree source
 *
 * @par
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bst.h"
#include "common.h"

/*!
 * @brief Create node
 *
 * @param[in] p_bst Pointer to BST
 * @param[in] size  Size of data in bytes
 *
 * @return Pointer to node
 */
static node_t * node_create(bst_t * p_bst, size_t size);

/*!
 * @brief Destroy node
 *
 * @param[in] p_bst  Pointer to BST
 * @param[in] p_node Pointer to node
 *
 * @return Status of operation
 */
static status_t node_destroy(bst_t * p_bst, node_t * p_node);

/*!
 * @brief Compare node data
 *
 * @param[in] p_bst   Pointer to BST
 * @param[in] p_data1 Pointer to first data
 * @param[in] p_data2 Pointer to second data
 * @param[in] size    Size of data in bytes
 *
 * @return Difference between first and second data
 */
static int node_compare(
    bst_t      * p_bst,
    void const * p_data1,
    void const * p_data2,
    size_t       size
);

bst_t *
bst_create (void)
{
    status_t status = STATUS_SUCCESS;

    bst_t * p_bst = calloc(1u, sizeof(*p_bst));
    if (NULL == p_bst)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_bst->p_root         = NULL;
    p_bst->len            = 0u;
    p_bst->p_display_data = NULL;
    p_bst->p_compare_data = NULL;
    p_bst->p_destroy_data = NULL;

cleanup:
    if (STATUS_SUCCESS != status)
    {
        bst_destroy(p_bst);
        p_bst = NULL;
    }
    return p_bst;
}

status_t
bst_destroy (bst_t * p_bst)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_bst)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    node_t * p_curr = p_bst->p_root;
    node_t * p_left = NULL;
    while (NULL != p_curr)
    {
        p_left = p_curr->p_left;
        node_destroy(p_bst, p_curr);
        p_curr = p_left;
    }

    p_bst->p_root         = NULL;
    p_bst->len            = 0u;
    p_bst->p_display_data = NULL;
    p_bst->p_compare_data = NULL;
    p_bst->p_destroy_data = NULL;

cleanup:
    free(p_bst);
    p_bst = NULL;
    return status;
}

status_t
bst_display (bst_t * p_bst, char const * p_sep)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_bst)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (NULL == p_bst->p_root)
    {
        status = STATUS_EMPTY;
        goto cleanup;
    }

    node_t * p_curr = p_bst->p_root;
    while (NULL != p_curr)
    {
        if (NULL != p_bst->p_display_data)
        {
            (p_bst->p_display_data)(*(void **)(p_curr->p_data));
        }
        else
        {
            display_printable(p_curr->p_data, p_curr->size, ", ", "\n");
        }

        if (NULL != p_curr->p_left)
        {
            printf("%s", p_sep);
        }

        p_curr = p_curr->p_left;
    }

cleanup:
    return status;
}

node_t *
bst_get (bst_t * p_bst, void const * p_data, size_t size)
{
    node_t * p_node = NULL;

    if ((NULL == p_bst) || (NULL == p_data))
    {
        goto cleanup;
    }

    node_t * p_curr = p_bst->p_root;
    while (NULL != p_curr)
    {
        // Compare node data to passed in data
        if (0 == node_compare(p_bst, p_curr->p_data, p_data, size))
        {
            p_node = p_curr;
            goto cleanup;
        }

        p_curr = p_curr->p_left;
    }

cleanup:
    return p_node;
}

status_t
bst_set (bst_t * p_bst, void const * p_data, size_t size)
{
    status_t status = STATUS_SUCCESS;
    UNUSED(size);

    if ((NULL == p_bst) || (NULL == p_data))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    node_t * p_node = node_create(p_bst, size);
    if (NULL == p_node)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memcpy(p_node->p_data, p_data, size);
    p_node->size = size;

    node_t * p_prev = NULL;
    node_t * p_curr = p_bst->p_root;

    while (NULL != p_curr)
    {
        // Compare node data to passed in data
        if (0 == node_compare(p_bst, p_curr->p_data, p_data, size))
        {
            break;
        }

        p_prev = p_curr;
        p_curr = p_curr->p_left;
    }

    // Previous node links to inserted node
    if (NULL == p_prev)
    {
        p_bst->p_root = p_node;
    }
    else
    {
        p_prev->p_left = p_node;
    }

    // Inserted node links to current node
    p_node->p_left = p_curr;

    (p_bst->len)++;

cleanup:
    return status;
}

status_t
bst_del (bst_t * p_bst, void const * p_data, size_t size)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_bst) || (NULL == p_data))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    node_t * p_prev = NULL;
    node_t * p_curr = p_bst->p_root;

    while (NULL != p_curr)
    {
        // Compare node data to passed in data
        if (0 == node_compare(p_bst, p_curr->p_data, p_data, size))
        {
            break;
        }

        p_prev = p_curr;
        p_curr = p_curr->p_left;
    }

    // Link skips node where data was found
    if (NULL == p_prev)
    {
        p_bst->p_root = p_curr->p_left;
    }
    else
    {
        p_prev->p_left = p_curr->p_left;
    }

    node_destroy(p_bst, p_curr);

    (p_bst->len)--;

cleanup:
    return status;
}

status_t
bst_foreach (bst_t * p_bst, bst_func_t p_func, void * p_ctx)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_bst) || (NULL == p_func))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    node_t * p_curr = p_bst->p_root;
    node_t * p_left = NULL;

    while (NULL != p_curr)
    {
        p_left = p_curr->p_left;

        status = p_func(p_curr, p_ctx);
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }

        p_curr = p_left;
    }

cleanup:
    return status;
}

static node_t *
node_create (bst_t * p_bst, size_t size)
{
    node_t * p_node = NULL;

    if (NULL == p_bst)
    {
        goto cleanup;
    }

    p_node = calloc(1u, sizeof(*p_node));
    if (NULL == p_node)
    {
        goto cleanup;
    }

    p_node->p_data = calloc(1u, size);
    if (NULL == p_node->p_data)
    {
        goto cleanup;
    }

    p_node->size    = size;
    p_node->p_left  = NULL;
    p_node->p_right = NULL;

cleanup:
    if (NULL == p_node)
    {
        node_destroy(p_bst, p_node);
        p_node = NULL;
    }
    return p_node;
}

static status_t
node_destroy (bst_t * p_bst, node_t * p_node)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_bst) || (NULL == p_node))
    {
        goto cleanup;
    }

    if (NULL != p_bst->p_destroy_data)
    {
        (p_bst->p_destroy_data)(*(void **)(p_node->p_data));
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
    bst_t      * p_bst,
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

    if (NULL == p_bst)
    {
        result = -1;
        goto cleanup;
    }

    if (NULL != p_bst->p_compare_data)
    {
        result = (p_bst->p_compare_data)(
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
