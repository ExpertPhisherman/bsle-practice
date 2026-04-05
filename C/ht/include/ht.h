/** @file ht.h
 *
 * @brief (U) Demonstrate skill in creating and using a hash table that accepts any data type:
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

#include "common.h"
#include "sll.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef HT_H
#define HT_H

typedef struct ht
{
    sll_t ** pp_entries;
    size_t capacity;
    size_t size;
    uint64_t (* hash)(void const * const, size_t const);
} ht_t;

/*!
 * @brief djb2 hash function
 *
 * @param[in] key     Pointer to key to be hashed
 * @param[in] key_len Key length
 *
 * @return 64-bit hash digest
 */
uint64_t djb2_hash(void const * const key, size_t const key_len);

/*!
 * @brief Create hash table
 *
 * @param[in] p_ht     Pointer to hash table
 * @param[in] capacity Maximum number of elements
 *
 * @return Status of operation
 */
status_t ht_create(ht_t * p_ht, size_t const capacity);

/*!
 * @brief Display hash table entries
 *
 * @param[in] p_ht   Pointer to hash table
 *
 * @return Status of operation
 */
status_t ht_display(ht_t * p_ht);

/*!
 * @brief Check if data in hash table
 *
 * @param[in] p_ht Pointer to hash table
 * @param[in] data Data to find
 *
 * @return Boolean if data in hash table
 */
bool ht_in(ht_t * p_ht, sll_data_t const data);

/*!
 * @brief Get SLL at index
 *
 * @param[in] p_ht  Pointer to hash table
 * @param[in] index Index to get SLL from
 * @param[in] p_sll Destination to send output
 *
 * @return Status of operation
 */
status_t ht_get(ht_t * p_ht, size_t index, sll_t * p_sll);

/*!
 * @brief Insert data into hash table
 *
 * @param[in] p_ht Pointer to hash table
 * @param[in] data Data to insert
 *
 * @return Status of operation
 */
status_t ht_insert(ht_t * p_ht, sll_data_t const data);

/*!
 * @brief Remove data from hash table
 *
 * @param[in] p_ht Pointer to hash table
 * @param[in] data Data to remove
 *
 * @return Status of operation
 */
status_t ht_remove(ht_t * p_ht, sll_data_t const data);

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
