/** @file ht.c
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

// TODO: Use a macro that prints when DEBUG macro is defined

#include "ht.h"

uint64_t
djb2_hash (void * key, size_t key_len)
{
    uint8_t chr;

    uint64_t hash = 5381u;

    // printf("Current key: ");
    for (size_t idx = 0u; idx < key_len; idx++)
    {
        chr = ((uint8_t *)key)[idx];
        // printf("%c", chr);
        hash = ((hash << 5u) + hash) + chr; // (hash * 33) + chr
    }
    // printf("\n");

    return hash;
}

status_t
ht_create (ht_t * p_ht, size_t capacity)
{
    status_t status;

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
    p_ht->hash = djb2_hash;

    // Allocate entries
    p_ht->pp_entries = calloc(capacity, sizeof(*(p_ht->pp_entries)));
    if (NULL == (p_ht->pp_entries))
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Allocate and set entries to empty SLLs
    for (size_t idx = 0u; idx < capacity; idx++)
    {
        // Allocate entry
        sll_t * p_sll = malloc(sizeof(*p_sll));
        if (NULL == p_sll)
        {
            status = STATUS_ALLOC_FAILURE;
            goto cleanup;
        }

        status = sll_create(p_sll);
        if (STATUS_NULL_ARG == status)
        {
            goto cleanup;
        }

        // Set entry
        (p_ht->pp_entries)[idx] = p_sll;
    }

    status = STATUS_SUCCESS;
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
    status_t status;

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
        sll_t * p_sll = (p_ht->pp_entries)[idx];
        if (NULL != (p_sll->p_head))
        {
            printf("%zu: ", idx);
            sll_display(p_sll);
        }
    }

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    return status;
}

bool
ht_in (ht_t * p_ht, void * p_data, size_t size)
{
    bool b_data_in;

    if (NULL == p_ht)
    {
        b_data_in = false;
        goto cleanup;
    }

    size_t capacity = p_ht->capacity;
    uint64_t hash = ((p_ht->hash)(p_data, size)) % capacity;

    b_data_in = sll_in((p_ht->pp_entries)[hash], p_data, size);
    goto cleanup;

cleanup:
    return b_data_in;
}

status_t
ht_get (ht_t * p_ht, size_t idx, sll_t * p_sll)
{
    status_t status;

    if (NULL == p_ht)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (idx >= p_ht->capacity)
    {
        status = STATUS_OVERFLOW;
        goto cleanup;
    }

    // Copy SLL into destination variable
    *p_sll = *((p_ht->pp_entries)[idx]);

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    return status;
}

status_t
ht_insert (ht_t * p_ht, void * p_data, size_t size)
{
    status_t status;

    if (NULL == p_ht)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    size_t capacity = p_ht->capacity;
    uint64_t hash = ((p_ht->hash)(p_data, size)) % capacity;

    sll_t * p_sll = (p_ht->pp_entries)[hash];

    // Insert data if not already exists in SLL
    if (!sll_in(p_sll, p_data, size))
    {
        // Append node
        status = sll_append(p_sll, p_data, size);

        // Increment size if first node inserted
        if (1u == (p_sll->len))
        {
            (p_ht->len)++;
        }

        goto cleanup;
    }

    // NOTE: Data already exists in SLL
    status = STATUS_EXISTS;
    goto cleanup;

cleanup:
    if (STATUS_ALLOC_FAILURE == status)
    {
        ht_destroy(p_ht);
    }

    return status;
}

status_t
ht_remove (ht_t * p_ht, void * p_data, size_t size)
{
    status_t status;

    if (NULL == p_ht)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    size_t capacity = p_ht->capacity;
    uint64_t hash = ((p_ht->hash)(p_data, size)) % capacity;

    sll_t * p_sll = (p_ht->pp_entries)[hash];

    // Remove data if exists in SLL
    if (sll_in(p_sll, p_data, size))
    {
        status = sll_remove(p_sll, p_data, size);

        // Decrement size if SLL is empty
        if (0u == (p_sll->len))
        {
            (p_ht->len)--;
        }

        goto cleanup;
    }

    // NOTE: Data does not exist in SLL
    status = STATUS_NOT_EXISTS;
    goto cleanup;

cleanup:
    if (STATUS_ALLOC_FAILURE == status)
    {
        ht_destroy(p_ht);
    }

    return status;
}

status_t
ht_destroy (ht_t * p_ht)
{
    status_t status;

    if (NULL == p_ht)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (NULL == (p_ht->pp_entries))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Free each entry
    for (size_t idx = 0u; idx < (p_ht->capacity); idx++)
    {
        sll_t * p_sll = (p_ht->pp_entries)[idx];

        // Free SLL nodes
        sll_destroy(p_sll);

        // Free SLL
        free(p_sll);
        p_sll = NULL;
    }

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    // Free all entries
    free(p_ht->pp_entries);
    p_ht->pp_entries = NULL;

    p_ht->capacity = 0u;
    p_ht->len = 0u;
    p_ht->hash = NULL;

    return status;
}

/*** end of file ***/
