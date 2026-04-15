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
    uint64_t   hash;       // Hash digest of key
    void     * p_key;      // Pointer to key
    size_t     key_size;   // Size of key in bytes
    void     * p_value;    // Pointer to value
    size_t     value_size; // Size of value in bytes
} item_t;

typedef uint64_t (*hash_func_t)(void * p_key, size_t key_size);

typedef struct ht
{
    sll_t          ** pp_buckets;     // Double pointer to buckets
    size_t            capacity;       // Current number of buckets
    size_t            len;            // Current occupied buckets
    hash_func_t       p_hash;         // Pointer to hash function
    display_func_t    p_display_item; // Pointer to display function
    cmp_func_t        p_cmp_item;     // Pointer to compare function
} ht_t;

/*!
 * @brief Create hash table
 *
 * @param[in] p_ht     Pointer to hash table
 * @param[in] capacity Maximum number of buckets
 *
 * @return Status of operation
 */
status_t ht_create(ht_t * p_ht, size_t capacity);

/*!
 * @brief Display hash table buckets
 *
 * @param[in] p_ht  Pointer to hash table
 * @param[in] p_sep Pointer to separator between each bucket
 *
 * @return Status of operation
 */
status_t ht_display(ht_t * p_ht, char const * p_sep);

/*!
 * @brief Get item at key in hash table
 *
 * @param[in] p_ht     Pointer to hash table
 * @param[in] p_key    Pointer to key to get
 * @param[in] key_size Size of key in bytes
 *
 * @return Pointer to found item
 */
item_t * ht_get(ht_t * p_ht, void * p_key, size_t key_size);

/*!
 * @brief Set item at key in hash table
 *
 * @param[in] p_ht       Pointer to hash table
 * @param[in] p_key      Pointer to key to set
 * @param[in] key_size   Size of key in bytes
 * @param[in] p_value    Pointer to value
 * @param[in] value_size Size of value in bytes
 *
 * @return Status of operation
 */
status_t ht_set(ht_t * p_ht,
                void * p_key, size_t key_size,
                void * p_value, size_t value_size);

/*!
 * @brief Delete item at key in hash table
 *
 * @param[in] p_ht     Pointer to hash table
 * @param[in] p_key    Pointer to key to delete
 * @param[in] key_size Size of key in bytes
 *
 * @return Status of operation
 */
status_t ht_del(ht_t * p_ht, void * p_key, size_t key_size);

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
