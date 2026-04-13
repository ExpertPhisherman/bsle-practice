/** @file ht.h
 *
 * @brief Hash table header
 *
 * @par
 * Creating a hash table with n number of items
 * Navigating through a hash table to find the nth item
 * Finding an item in a hash table
 * Removing selected items from a hash table
 * Inserting an item into a hash table
 * Implement functionality to mitigate hash collisions within the hash table
 * Removing all items from the hash table
 */

#ifndef HT_H
#define HT_H

#include "sll.h"
#include "common.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct item
{
    void     * p_key;      // Pointer to key
    size_t     key_size;   // Size of key in bytes
    void     * p_value;    // Pointer to value
    size_t     value_size; // Size of value in bytes
} item_t;

typedef uint64_t (*hash_func_t)(void * p_key, size_t key_size);

typedef struct ht
{
    sll_t          ** pp_elements;    // Double pointer to elements
    size_t            capacity;       // Maximum number of elements
    size_t            len;            // Current length
    hash_func_t       p_hash_func;    // Pointer to hash function
    display_func_t    p_display_func; // Pointer to display function
    cmp_func_t        p_cmp_func;     // Pointer to compare function
} ht_t;

/*!
 * @brief Create hash table
 *
 * @param[in] p_ht     Pointer to hash table
 * @param[in] capacity Maximum number of elements
 *
 * @return Status of operation
 */
status_t ht_create(ht_t * p_ht, size_t capacity);

/*!
 * @brief Display hash table elements
 *
 * @param[in] p_ht  Pointer to hash table
 * @param[in] p_sep Pointer to separator between each element
 *
 * @return Status of operation
 */
status_t ht_display(ht_t * p_ht, char const * p_sep);

/*!
 * @brief Check if key in hash table
 *
 * @param[in] p_ht     Pointer to hash table
 * @param[in] p_key    Pointer to key to find
 * @param[in] key_size Size of key in bytes
 *
 * @return Pointer to found node
 */
node_t * ht_in(ht_t * p_ht, void * p_key, size_t key_size);

/*!
 * @brief Insert item into hash table
 *
 * @param[in] p_ht       Pointer to hash table
 * @param[in] p_key      Pointer to key to find
 * @param[in] key_size   Size of key in bytes
 * @param[in] p_value    Pointer to value
 * @param[in] value_size Size of value in bytes
 *
 * @return Status of operation
 */
status_t ht_insert(ht_t * p_ht,
                   void * p_key, size_t key_size,
                   void * p_value, size_t value_size);

/*!
 * @brief Remove item from hash table
 *
 * @param[in] p_ht     Pointer to hash table
 * @param[in] p_key    Pointer to key to find
 * @param[in] key_size Size of key in bytes
 *
 * @return Status of operation
 */
status_t ht_remove(ht_t * p_ht, void * p_key, size_t key_size);

/*!
 * @brief Destroy hash table
 *
 * @param[in] p_ht Pointer to hash table
 *
 * @return Status of operation
 */
status_t ht_destroy(ht_t * p_ht);

#endif /* HT_H */

/*** end of file ***/
