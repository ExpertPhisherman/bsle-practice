/** @file sll.h
 *
 * @brief Singly linked list header
 *
 */

#ifndef SLL_H
#define SLL_H

#include "common.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct node
{
    void        * p_data; // Pointer to data
    size_t        size;   // Size of data in bytes
    struct node * p_next; // Pointer to next node
} node_t;

typedef struct sll
{
    node_t * p_head; // Pointer to head node
    size_t   len;    // Current length
} sll_t;

/*!
 * @brief Create SLL
 *
 * @param[in] p_sll Pointer to SLL
 *
 * @return Status of operation
 */
status_t sll_create(sll_t * p_sll);

/*!
 * @brief Display SLL
 *
 * @param[in] p_sll Pointer to SLL
 *
 * @return Status of operation
 */
status_t sll_display(sll_t * p_sll);

/*!
 * @brief Check if data in SLL
 *
 * @param[in] p_sll  Pointer to SLL
 * @param[in] p_data Pointer to data to find
 * @param[in] size   Size of data in bytes
 *
 * @return Boolean if data in SLL
 */
bool sll_in(sll_t * p_sll, void * p_data, size_t size);

/*!
 * @brief Insert data into SLL
 *
 * @param[in] p_sll  Pointer to SLL
 * @param[in] p_data Pointer to data to insert
 * @param[in] size   Size of data in bytes
 * @param[in] idx    Index to insert data at
 * @return Status of operation
 */
status_t sll_insert(sll_t * p_sll, void * p_data, size_t size, size_t idx);

/*!
 * @brief Append data to SLL
 *
 * @param[in] p_sll  Pointer to SLL
 * @param[in] p_data Pointer to data to append
 * @param[in] size   Size of data in bytes
 * @return Status of operation
 */
status_t sll_append(sll_t * p_sll, void * p_data, size_t size);

/*!
 * @brief Remove data from SLL
 *
 * @param[in] p_sll  Pointer to SLL
 * @param[in] p_data Pointer to data to remove
 * @param[in] size   Size of data in bytes
 *
 * @return Status of operation
 */
status_t sll_remove(sll_t * p_sll, void * p_data, size_t size);

/*!
 * @brief Destroy SLL
 *
 * @param[in] p_sll Pointer to SLL
 *
 * @return Status of operation
 */
status_t sll_destroy(sll_t * p_sll);

#endif /* SLL_H */

/*** end of file ***/
