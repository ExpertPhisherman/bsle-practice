/** @file ht.h
 *
 * @brief Hash table header
 *
 * @par
 *
 */

#ifndef HT_H
#define HT_H

#include <stddef.h>
#include <stdint.h>

#include "common.h"

typedef struct ht   ht_t;
typedef struct sll  sll_t;
typedef struct item item_t;

typedef uint64_t (*hash_func_t)(void const * p_key, size_t key_size);
typedef status_t (*ht_func_t)(item_t * p_item, void * p_ctx);

typedef struct ht_ctx
{
    ht_func_t   p_func; // Pointer to caller item function
    void      * p_ctx;  // Pointer to caller context
} ht_ctx_t;

typedef struct item
{
    ht_t     * p_ht;       // Pointer to hash table
    uint64_t   hash;       // Hash digest of key
    void     * p_key;      // Pointer to key
    size_t     key_size;   // Size of key in bytes
    void     * p_value;    // Pointer to value
    size_t     value_size; // Size of value in bytes
} item_t;

typedef struct ht
{
    sll_t          ** pp_buckets;      // Pointer to bucket array
    size_t            capacity;        // Current number of buckets
    size_t            len;             // Current occupied buckets
    hash_func_t       p_hash_func;     // Pointer to hash function
    destroy_func_t    p_destroy_key;   // Pointer to destroy key function
    destroy_func_t    p_destroy_value; // Pointer to destroy value function
} ht_t;

/*!
 * @brief Create hash table
 *
 * @param[in] capacity Current number of buckets
 *
 * @return Pointer to hash table
 */
ht_t * ht_create(size_t capacity);

/*!
 * @brief Destroy hash table
 *
 * @param[in] p_ht Pointer to hash table
 *
 * @return Status of operation
 */
status_t ht_destroy(ht_t * p_ht);

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
item_t * ht_get(ht_t * p_ht, void const * p_key, size_t key_size);

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
status_t ht_set(
    ht_t       * p_ht,
    void const * p_key,
    size_t       key_size,
    void const * p_value,
    size_t       value_size
);

/*!
 * @brief Delete item at key in hash table
 *
 * @param[in] p_ht     Pointer to hash table
 * @param[in] p_key    Pointer to key to delete
 * @param[in] key_size Size of key in bytes
 *
 * @return Status of operation
 */
status_t ht_del(ht_t * p_ht, void const * p_key, size_t key_size);

/*!
 * @brief Apply function to each item in hash table
 *
 * @param[in] p_ht   Pointer to hash table
 * @param[in] p_func Pointer to function applied to each item
 * @param[in] p_ctx  Pointer to caller context
 *
 * @return Status of operation
 */
status_t ht_foreach(ht_t * p_ht, ht_func_t p_func, void * p_ctx);

#endif /* HT_H */

/*** end of file ***/
