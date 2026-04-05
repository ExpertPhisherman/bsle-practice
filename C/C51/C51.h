/** @file C51.h
 *
 * @brief (U) Demonstrate skill in creating and using a circularly linked list that accepts any data type:
 *
 * @par
 * Creating a circularly linked list with n number of items
 * Navigating through a circularly linked list
 * Finding the first occurrence of an item in a circularly linked list
 * Sorting the circularly linked list alphanumerically using a function pointer
 * Removing selected items from the circularly linked list
 * Inserting an item into a specific location in a circularly linked list
 * Removing all items from the circularly linked list
 * Destroying a circularly linked list
 * "Circular linked list" will be abbreviated as "CLL" or "cll"
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef C51_H
#define C51_H

// NOTE: Change accordingly
typedef char * cll_data_t;

typedef struct node
{
    void * p_data;
    struct node * p_next;
} node;

/*!
 * @brief Creating a circularly linked list with n number of items
 *
 * @param[in] p_bytes    Bytes to write
 * @param[in] node_count Number of nodes
 * @param[in] data_size  Size of data in bytes
 *
 * @return Pointer to head node
 */
node * cll_init(const void * p_bytes, const size_t node_count, const size_t data_size);

/*!
 * @brief Navigating through a circularly linked list
 *
 * @param[in] p_head   Pointer to head node
 * @param[in] p_format Format specifier
 *
 * @return void
 */
void cll_display(node * p_head, const char * p_format);

/*!
 * @brief Finding the first occurrence of an item in a circularly linked list
 *
 * @param[in] p_head    Pointer to head node
 * @param[in] p_data    Pointer to data to find
 * @param[in] data_size Size of data in bytes
 * @param[in] p_key     Pointer to index of data found
 *
 * @return Pointer to node with data found
 */
node * cll_find(node * p_head, const void * p_data, const size_t data_size, size_t * p_key);

/*!
 * @brief Compare function for qsort
 *
 * @param[in] p_data_x Pointer to first data
 * @param[in] p_data_y Pointer to second data
 *
 * @return Result of comparison (negative: first < second, zero: first == second, positive: first > second)
 */
int compar(const void * p_data_x, const void * p_data_y);

/*!
 * @brief Sorting the circularly linked list alphanumerically using a function pointer
 *
 * @param[in] p_head     Pointer to head node
 * @param[in] node_count Number of nodes
 * @param[in] data_size  Size of data in bytes
 * @param[in] p_compar   Pointer to function used to sort
 *
 * @return Pointer to head node after sort
 */
node * cll_sort(node * p_head, const size_t node_count, const size_t data_size, int (* p_compar)(const void * p_data_x, const void * p_data_y));

/*!
 * @brief Removing selected items from the circularly linked list
 *
 * @param[in] p_head       Pointer to head node
 * @param[in] p_data       Pointer to data to find
 * @param[in] data_size    Size of data in bytes
 * @param[in] p_node_count Pointer to number of nodes
 *
 * @return Pointer to head node after node removed
 */
node * cll_remove(node * p_head, const void * p_data, const size_t data_size, size_t * p_node_count);

/*!
 * @brief Inserting an item into a specific location in a circularly linked list
 *
 * @param[in] p_head       Pointer to head node
 * @param[in] p_data       Pointer to data to find
 * @param[in] data_size    Size of data in bytes
 * @param[in] p_node_count Pointer to number of nodes
 * @param[in] p_key        Pointer to index to insert
 *
 * @return Pointer to head node after node inserted
 */
node * cll_insert(node * p_head, const void * p_data, const size_t data_size, size_t * p_node_count, const size_t * p_key);

/*!
 * @brief Destroying a circularly linked list
 *
 * @param[in] p_head Pointer to head node
 *
 * @return void
 */
void cll_destroy(node * p_head);

#endif /* C51_H */

/*** end of file ***/
