/** @file bst.c
 *
 * @brief Binary search tree source
 *
 * @par
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "bst.h"

/*!
 * @brief Get the link that holds (or will hold) the node with data
 *
 * @param[in] p_bst  Pointer to BST
 * @param[in] p_data Pointer to data
 * @param[in] size   Size of data in bytes
 *
 * @return Pointer to link
 */
static node_t ** get_link(
    bst_t      * p_bst,
    void const * p_data,
    size_t       size
);

/*!
 * @brief Recursively destroy subtree with root at node
 *
 * @param[in] p_bst  Pointer to BST
 * @param[in] p_node Pointer to subtree root node
 *
 * @return Status of operation
 */
static status_t subtree_destroy(bst_t * p_bst, node_t * p_node);

/*!
 * @brief Recursively apply function to each node of subtree
 *
 * @param[in] p_node Pointer to subtree root node
 * @param[in] p_func Pointer to function applied to each node
 * @param[in] p_ctx  Pointer to caller context
 *
 * @return Status of operation
 */
static status_t subtree_foreach(
    node_t     * p_node,
    bst_func_t   p_func,
    void       * p_ctx
);

/*!
 * @brief Display single node
 *
 * @param[in] p_node Pointer to node
 * @param[in] p_ctx  Pointer to display context
 *
 * @return Status of operation
 */
static status_t node_display(node_t * p_node, void * p_ctx);

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
 * @param[in] size1   Size of first data in bytes
 * @param[in] size2   Size of second data in bytes
 *
 * @return Difference between first and second data
 */
static int data_compare(
    bst_t      * p_bst,
    void const * p_data1,
    void const * p_data2,
    size_t       size1,
    size_t       size2
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

    subtree_destroy(p_bst, p_bst->p_root);

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

    if ((NULL == p_bst) || (NULL == p_bst->p_root) || (NULL == p_sep))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    display_ctx_t ctx =
    {
        .p_bst   = p_bst,
        .p_sep   = p_sep,
        .b_first = true
    };

    status = bst_foreach(p_bst, node_display, &ctx);
    printf("\n");

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

    node_t ** pp_link = get_link(p_bst, p_data, size);
    if (NULL == pp_link)
    {
        goto cleanup;
    }

    p_node = *pp_link;

cleanup:
    return p_node;
}

status_t
bst_set (bst_t * p_bst, void const * p_data, size_t size)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_bst) || (NULL == p_data))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    node_t ** pp_link = get_link(p_bst, p_data, size);
    if (NULL == pp_link)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (NULL != *pp_link)
    {
        status = STATUS_EXISTS;
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

    *pp_link = p_node;

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

    node_t ** pp_link = get_link(p_bst, p_data, size);
    if (NULL == pp_link)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    node_t * p_target = *pp_link;
    if (NULL == p_target)
    {
        status = STATUS_NOT_EXISTS;
        goto cleanup;
    }

    if ((NULL != p_target->p_left) && (NULL != p_target->p_right))
    {
        // NOTE: Two children exist
        // Replace target with successor node
        node_t ** pp_next = &(p_target->p_right);
        while (NULL != (*pp_next)->p_left)
        {
            pp_next = &((*pp_next)->p_left);
        }

        node_t * p_next = *pp_next;

        // Unlink successor node
        *pp_next = p_next->p_right;

        p_next->p_left  = p_target->p_left;
        p_next->p_right = p_target->p_right;

        *pp_link = p_next;
    }
    else if (NULL != p_target->p_left)
    {
        // NOTE: One child exists
        *pp_link = p_target->p_left;
    }
    else
    {
        // NOTE: One child or none exists
        *pp_link = p_target->p_right;
    }

    p_target->p_left  = NULL;
    p_target->p_right = NULL;

    status = node_destroy(p_bst, p_target);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

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

    status = subtree_foreach(p_bst->p_root, p_func, p_ctx);

cleanup:
    return status;
}

static node_t **
get_link (bst_t * p_bst, void const * p_data, size_t size)
{
    node_t ** pp_link = NULL;

    if ((NULL == p_bst) || (NULL == p_data))
    {
        goto cleanup;
    }

    pp_link = &(p_bst->p_root);

    while (NULL != *pp_link)
    {
        node_t * p_link = *pp_link;

        int result = data_compare(
            p_bst,
            p_data,
            p_link->p_data,
            size,
            p_link->size
        );

        if (0 == result)
        {
            break;
        }

        pp_link = (0 > result) ? &(p_link->p_left) : &(p_link->p_right);
    }

cleanup:
    return pp_link;
}

static status_t
subtree_destroy (bst_t * p_bst, node_t * p_node)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_bst)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (NULL == p_node)
    {
        goto cleanup;
    }

    subtree_destroy(p_bst, p_node->p_left);
    subtree_destroy(p_bst, p_node->p_right);

    p_node->p_left  = NULL;
    p_node->p_right = NULL;

    status = node_destroy(p_bst, p_node);

cleanup:
    return status;
}

static status_t
subtree_foreach (node_t * p_node, bst_func_t p_func, void * p_ctx)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_func)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Separate check for base case
    if (NULL == p_node)
    {
        goto cleanup;
    }

    // Visit nodes in order: left, current, right

    status = subtree_foreach(p_node->p_left, p_func, p_ctx);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    status = p_func(p_node, p_ctx);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    status = subtree_foreach(p_node->p_right, p_func, p_ctx);

cleanup:
    return status;
}

static status_t
node_display (node_t * p_node, void * p_ctx)
{
    status_t status = STATUS_SUCCESS;

    display_ctx_t * p_display = p_ctx;

    if ((NULL == p_node) || (NULL == p_display))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (!p_display->b_first)
    {
        printf("%s", p_display->p_sep);
    }
    p_display->b_first = false;

    if (NULL != p_display->p_bst->p_display_data)
    {
        (p_display->p_bst->p_display_data)(*(void **)(p_node->p_data));
    }
    else
    {
        display_printable(p_node->p_data, p_node->size, "", "");
    }

cleanup:
    return status;
}

static node_t *
node_create (bst_t * p_bst, size_t size)
{
    status_t status = STATUS_SUCCESS;

    node_t * p_node = NULL;

    if (NULL == p_bst)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_node = calloc(1u, sizeof(*p_node));
    if (NULL == p_node)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_node->p_data = calloc(1u, size);
    if (NULL == p_node->p_data)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_node->size    = size;
    p_node->p_left  = NULL;
    p_node->p_right = NULL;

cleanup:
    if (STATUS_SUCCESS != status)
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
        status = STATUS_NULL_ARG;
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
data_compare (
    bst_t      * p_bst,
    void const * p_data1,
    void const * p_data2,
    size_t       size1,
    size_t       size2
)
{
    int result = 0;

    if (NULL == p_bst)
    {
        goto cleanup;
    }

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

    size_t min_size = umin(size1, size2);

    if (NULL != p_bst->p_compare_data)
    {
        result = (p_bst->p_compare_data)(
            *(void **)p_data1,
            *(void **)p_data2,
            min_size
        );
        goto cleanup;
    }

    result = memcmp(p_data1, p_data2, min_size);

    if (0 == result)
    {
        if (size1 < size2)
        {
            result = -1;
        }
        else if (size1 > size2)
        {
            result = 1;
        }
    }

cleanup:
    return result;
}

/*** end of file ***/
