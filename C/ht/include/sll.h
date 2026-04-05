/** @file sll.h
 *
 * @brief Singly linked list
 *
 */

#include "common.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SLL_H
#define SLL_H

// NOTE: Change accordingly
typedef char const * sll_data_t;

typedef struct node
{
    void * p_data;
    struct node * p_next;
} node_t;

typedef struct
{
    node_t * p_head;
    size_t size;
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
 * @param[in] p_sll Pointer to SLL
 * @param[in] data  Data to find
 *
 * @return Boolean if data in SLL
 */
bool sll_in(sll_t * p_sll, sll_data_t const data);

/*!
 * @brief Insert data into SLL
 *
 * @param[in] p_sll Pointer to SLL
 * @param[in] data  Data to insert
 * @param[in] index Index to insert
 *
 * @return Status of operation
 */
status_t sll_insert(sll_t * p_sll, sll_data_t const data, size_t index);

/*!
 * @brief Remove data from SLL
 *
 * @param[in] p_sll Pointer to SLL
 * @param[in] data  Data to remove
 *
 * @return Status of operation
 */
status_t sll_remove(sll_t * p_sll, sll_data_t const data);

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
