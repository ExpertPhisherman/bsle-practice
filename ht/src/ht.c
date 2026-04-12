/** @file ht.c
 *
 * @brief Hash table source
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

#include "ht.h"

/*!
 * @brief djb2 hash function
 *
 * @param[in] p_key    Pointer to key to be hashed
 * @param[in] key_size Size of key in bytes
 *
 * @return 64-bit hash digest
 */
static uint64_t djb2_hash(void * p_key, size_t key_size);

/*!
 * @brief Select SLL containing key in hash table
 *
 * @param[in] p_ht     Pointer to hash table
 * @param[in] p_key    Pointer to key to find
 * @param[in] key_size Size of key in bytes
 *
 * @return Pointer to SLL containing key
 */
static sll_t * ht_select(ht_t * p_ht, void * p_key, size_t key_size);

status_t
ht_create (ht_t * p_ht, size_t capacity)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_ht)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (0u == capacity)
    {
        status = STATUS_EMPTY;
        goto cleanup;
    }

    p_ht->capacity = capacity;
    p_ht->len = 0u;
    p_ht->p_hash_func = djb2_hash;

    p_ht->pp_elements = calloc(capacity, sizeof(*(p_ht->pp_elements)));
    if (NULL == p_ht->pp_elements)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    for (size_t idx = 0u; idx < capacity; idx++)
    {
        sll_t * p_sll = malloc(sizeof(*p_sll));
        if (NULL == p_sll)
        {
            status = STATUS_ALLOC_FAILURE;
            goto cleanup;
        }

        sll_create(p_sll);

        // Set element to empty SLL
        (p_ht->pp_elements)[idx] = p_sll;
    }

    goto cleanup;

cleanup:
    if (STATUS_ALLOC_FAILURE == status)
    {
        ht_destroy(p_ht);
    }

    return status;
}

status_t
ht_display (ht_t * p_ht)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_ht)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (0u == p_ht->len)
    {
        status = STATUS_EMPTY;
        goto cleanup;
    }

    for (size_t idx = 0u; idx < p_ht->capacity; idx++)
    {
        sll_t * p_sll = (p_ht->pp_elements)[idx];
        if (NULL != p_sll->p_head)
        {
            printf("%zu: ", idx);
            sll_display(p_sll);
        }
    }

    goto cleanup;

cleanup:
    return status;
}

bool
ht_in (ht_t * p_ht, void * p_key, size_t key_size)
{
    bool b_key_in;

    if ((NULL == p_ht) || (NULL == p_key))
    {
        b_key_in = false;
        goto cleanup;
    }

    sll_t * p_sll = ht_select(p_ht, p_key, key_size);

    b_key_in = sll_in(p_sll, p_key, key_size);

    goto cleanup;

cleanup:
    return b_key_in;
}

status_t
ht_insert (ht_t * p_ht, void * p_key, size_t key_size)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_ht) || (NULL == p_key))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sll_t * p_sll = ht_select(p_ht, p_key, key_size);

    // Insert data if it does not exist in SLL
    if (sll_in(p_sll, p_key, key_size))
    {
        // NOTE: Data exists in SLL
        status = STATUS_EXISTS;
        goto cleanup;
    }

    status = sll_append(p_sll, p_key, key_size);

    // Increment length if first node inserted
    if (1u == p_sll->len)
    {
        (p_ht->len)++;
    }

    goto cleanup;

cleanup:
    if (STATUS_ALLOC_FAILURE == status)
    {
        ht_destroy(p_ht);
    }

    return status;
}

status_t
ht_remove (ht_t * p_ht, void * p_key, size_t key_size)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_ht) || (NULL == p_key))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sll_t * p_sll = ht_select(p_ht, p_key, key_size);

    // Remove data if it exists in SLL
    if (!sll_in(p_sll, p_key, key_size))
    {
        // NOTE: Data does not exist in SLL
        status = STATUS_NOT_EXISTS;
        goto cleanup;
    }

    status = sll_remove(p_sll, p_key, key_size);

    // Decrement length if last node removed
    if (0u == p_sll->len)
    {
        (p_ht->len)--;
    }

    goto cleanup;

cleanup:
    return status;
}

status_t
ht_destroy (ht_t * p_ht)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_ht)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_ht->len = 0u;
    p_ht->p_hash_func = NULL;

    if (NULL == p_ht->pp_elements)
    {
        p_ht->capacity = 0u;
        goto cleanup;
    }

    // Free each element
    for (size_t idx = 0u; idx < p_ht->capacity; idx++)
    {
        sll_t * p_sll = (p_ht->pp_elements)[idx];
        (p_ht->pp_elements)[idx] = NULL;

        sll_destroy(p_sll);
        free(p_sll);
        p_sll = NULL;
    }

    p_ht->capacity = 0u;

    // Free all elements
    free(p_ht->pp_elements);
    p_ht->pp_elements = NULL;

    goto cleanup;

cleanup:
    return status;
}

static uint64_t
djb2_hash (void * p_key, size_t key_size)
{
    uint8_t chr;
    uint64_t hash = 5381u;

    DEBUG_PRINT("Current key: ");
    for (size_t idx = 0u; idx < key_size; idx++)
    {
        chr = ((uint8_t *)p_key)[idx];
        DEBUG_PRINT("%c", chr);
        hash = ((hash << 5u) + hash) + chr; // (hash * 33) + chr
    }
    DEBUG_PRINT("\n");

    return hash;
}

static sll_t *
ht_select (ht_t * p_ht, void * p_key, size_t key_size)
{
    sll_t * p_sll = NULL;

    if ((NULL == p_ht) || (NULL == p_key))
    {
        goto cleanup;
    }

    uint64_t hash = (p_ht->p_hash_func)(p_key, key_size);
    p_sll = (p_ht->pp_elements)[hash % p_ht->capacity];

    goto cleanup;

cleanup:
    return p_sll;
}

/*** end of file ***/
