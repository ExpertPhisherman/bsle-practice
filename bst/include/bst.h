/** @file bst.h
 *
 * @brief Binary search tree header
 *
 * @par
 *
 */

#ifndef BST_H
#define BST_H

#include <stddef.h>

#include "common.h"

typedef struct node node_t;

typedef status_t (*bst_func_t)(node_t * p_node, void * p_ctx);

typedef struct node
{
    void   * p_data;  // Pointer to data
    size_t   size;    // Size of data in bytes
    node_t * p_left;  // Pointer to left node
    node_t * p_right; // Pointer to right node
} node_t;

typedef struct bst
{
    node_t         * p_root;         // Pointer to root node
    size_t           len;            // Current length
    display_func_t   p_display_data; // Pointer to display function
    compare_func_t   p_compare_data; // Pointer to compare function
    destroy_func_t   p_destroy_data; // Pointer to destroy function
} bst_t;

typedef struct display_ctx
{
    bst_t      * p_bst;   // Pointer to BST
    char const * p_sep;   // Pointer to separator between each node
    bool         b_first; // Whether the next node is the first printed
} display_ctx_t;

/*!
 * @brief Create BST
 *
 * @param[in] void
 *
 * @return Pointer to BST
 */
bst_t * bst_create(void);

/*!
 * @brief Destroy BST
 *
 * @param[in] p_bst Pointer to BST
 *
 * @return Status of operation
 */
status_t bst_destroy(bst_t * p_bst);

/*!
 * @brief Display BST
 *
 * @param[in] p_bst Pointer to BST
 * @param[in] p_sep Pointer to separator between each node
 *
 * @return Status of operation
 */
status_t bst_display(bst_t * p_bst, char const * p_sep);

/*!
 * @brief Get node with data in BST
 *
 * @param[in] p_bst  Pointer to BST
 * @param[in] p_data Pointer to data to get
 * @param[in] size   Size of data in bytes
 *
 * @return Pointer to found node
 */
node_t * bst_get(bst_t * p_bst, void const * p_data, size_t size);

/*!
 * @brief Set node with data into BST
 *
 * @param[in] p_bst  Pointer to BST
 * @param[in] p_data Pointer to data to set
 * @param[in] size   Size of data in bytes
 *
 * @return Status of operation
 */
status_t bst_set(bst_t * p_bst, void const * p_data, size_t size);

/*!
 * @brief Delete data from BST
 *
 * @param[in] p_bst  Pointer to BST
 * @param[in] p_data Pointer to data to delete
 * @param[in] size   Size of data in bytes
 *
 * @return Status of operation
 */
status_t bst_del(bst_t * p_bst, void const * p_data, size_t size);

/*!
 * @brief Apply function to each node in BST
 *
 * @param[in] p_bst  Pointer to BST
 * @param[in] p_func Pointer to function applied to each node
 * @param[in] p_ctx  Pointer to caller context
 *
 * @return Status of operation
 */
status_t bst_foreach(bst_t * p_bst, bst_func_t p_func, void * p_ctx);

#endif /* BST_H */

/*** end of file ***/
