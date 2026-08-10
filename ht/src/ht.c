/** @file ht.c
 *
 * @brief Hash table source
 *
 * @par
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "ht.h"
#include "sll.h"

/*!
 * @brief djb2 hash function
 *
 * @par
 * Reference:
 * https://www.cse.yorku.ca/~oz/hash.html
 *
 * @param[in] p_key    Pointer to key to be hashed
 * @param[in] key_size Size of key in bytes
 *
 * @return 64-bit hash digest
 */
static uint64_t djb2_hash(void const * p_key, size_t key_size);

/*!
 * @brief Select SLL containing key in hash table
 *
 * @param[in] p_ht   Pointer to hash table
 * @param[in] p_item Pointer to item to find
 *
 * @return Pointer to SLL containing key
 */
static sll_t * ht_select(ht_t * p_ht, item_t * p_item);

/*!
 * @brief Display item
 *
 * @param[in] p_data Pointer to item
 *
 * @return Status of operation
 */
static status_t item_display(void const * p_data);

/*!
 * @brief Compare items
 *
 * @param[in] p_data1 Pointer to first item
 * @param[in] p_data2 Pointer to second item
 * @param[in] size    Size of item in bytes
 *
 * @return Difference between first and second item
 */
static int item_compare(
    void const * p_data1,
    void const * p_data2,
    size_t size
);

/*!
 * @brief Destroy item
 *
 * @param[in] p_data Pointer to item
 *
 * @return void
 */
static void item_destroy(void * p_data);

/*!
 * @brief Apply caller item function to item in node
 *
 * @param[in] p_node Pointer to node containing item pointer
 * @param[in] p_ctx  Pointer to hash table context
 *
 * @return Status of operation
 */
static status_t item_foreach(node_t * p_node, void * p_ctx);

ht_t *
ht_create (size_t capacity)
{
    status_t status = STATUS_SUCCESS;

    ht_t * p_ht = NULL;

    if (0u == capacity)
    {
        status = STATUS_EMPTY;
        goto cleanup;
    }

    p_ht = calloc(1u, sizeof(*p_ht));
    if (NULL == p_ht)
    {
        fprintf(stderr, "calloc failed in ht_create\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_ht->capacity        = capacity;
    p_ht->len             = 0u;
    p_ht->p_hash_func     = djb2_hash;
    p_ht->p_destroy_key   = NULL;
    p_ht->p_destroy_value = NULL;

    p_ht->pp_buckets = calloc(capacity, sizeof(*(p_ht->pp_buckets)));
    if (NULL == p_ht->pp_buckets)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    for (size_t idx = 0u; idx < capacity; idx++)
    {
        sll_t * p_sll = sll_create();
        if (NULL == p_sll)
        {
            status = STATUS_ALLOC_FAILURE;
            goto cleanup;
        }

        p_sll->p_display_data = item_display;
        p_sll->p_compare_data = item_compare;
        p_sll->p_destroy_data = item_destroy;

        // Set bucket to empty SLL
        (p_ht->pp_buckets)[idx] = p_sll;
    }

cleanup:
    if (STATUS_SUCCESS != status)
    {
        ht_destroy(p_ht);
        p_ht = NULL;
    }

    return p_ht;
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

    p_ht->len         = 0u;
    p_ht->p_hash_func = NULL;

    if (NULL == p_ht->pp_buckets)
    {
        p_ht->capacity = 0u;
        goto cleanup;
    }

    // Free each bucket
    for (size_t idx = 0u; idx < p_ht->capacity; idx++)
    {
        sll_destroy((p_ht->pp_buckets)[idx]);
        (p_ht->pp_buckets)[idx] = NULL;
    }

    p_ht->p_destroy_key   = NULL;
    p_ht->p_destroy_value = NULL;
    p_ht->capacity        = 0u;

    free(p_ht->pp_buckets);
    p_ht->pp_buckets = NULL;

cleanup:
    free(p_ht);
    p_ht = NULL;
    return status;
}

status_t
ht_display (ht_t * p_ht, char const * p_sep)
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

    bool b_first = true;

    printf("{");
    for (size_t idx = 0u; idx < p_ht->capacity; idx++)
    {
        sll_t * p_sll = (p_ht->pp_buckets)[idx];
        if (NULL != p_sll->p_head)
        {
            if (!b_first)
            {
                // Print separator between each non-empty bucket
                printf("%s", p_sep);
            }
            sll_display(p_sll, p_sep);
            b_first = false;
        }
    }
    printf("}\n");

cleanup:
    return status;
}

item_t *
ht_get (ht_t * p_ht, void const * p_key, size_t key_size)
{
    item_t * p_item = NULL;

    if ((NULL == p_ht) || (NULL == p_key) || (NULL == p_ht->p_hash_func))
    {
        goto cleanup;
    }

    item_t probe =
    {
        .p_ht       = p_ht,
        .hash       = (p_ht->p_hash_func)(p_key, key_size),
        .p_key      = (void *)p_key,
        .key_size   = key_size,
        .p_value    = NULL,
        .value_size = 0u,
    };

    item_t * p_probe = &probe;

    sll_t  * p_sll  = ht_select(p_ht, p_probe);
    node_t * p_node = sll_get(p_sll, &p_probe, sizeof(p_probe));

    if (NULL == p_node)
    {
        goto cleanup;
    }

    p_item = *(item_t **)(p_node->p_data);

cleanup:
    return p_item;
}

status_t
ht_set (
    ht_t       * p_ht,
    void const * p_key,
    size_t       key_size,
    void const * p_value,
    size_t       value_size
)
{
    status_t status = STATUS_SUCCESS;

    item_t * p_new = NULL;

    if (
        (NULL == p_ht) ||
        (NULL == p_key) ||
        (NULL == p_value) ||
        (NULL == p_ht->p_hash_func)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_new = calloc(1u, sizeof(*p_new));
    if (NULL == p_new)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_new->p_ht       = p_ht;
    p_new->hash       = (p_ht->p_hash_func)(p_key, key_size);
    p_new->key_size   = key_size;
    p_new->value_size = value_size;

    // Allocate hash table owned key and value
    p_new->p_key = calloc(1u, key_size);
    if (NULL == p_new->p_key)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    memcpy(p_new->p_key, p_key, key_size);

    p_new->p_value = calloc(1u, value_size);
    if (NULL == p_new->p_value)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    memcpy(p_new->p_value, p_value, value_size);

    sll_t  * p_sll  = ht_select(p_ht, p_new);
    node_t * p_node = sll_get(p_sll, &p_new, sizeof(p_new));

    // Update item if key exists in SLL
    if (NULL != p_node)
    {
        // NOTE: Key exists in SLL
        item_t * p_item = *(item_t **)(p_node->p_data);

        // Destroy old value
        if (NULL != p_ht->p_destroy_value)
        {
            (p_ht->p_destroy_value)(*(void **)(p_item->p_value));
        }

        free(p_item->p_value);
        p_item->p_value = NULL;

        // Transfer new value to existing item
        p_item->p_value    = p_new->p_value;
        p_item->value_size = p_new->value_size;
        p_new->p_value     = NULL;

        status = STATUS_EXISTS;
        goto cleanup;
    }

    // Append item if key does not exist in SLL
    status = sll_append(p_sll, &p_new, sizeof(p_new));
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    // Transfer ownership of item to SLL
    p_new = NULL;

    // Increment length if first node inserted
    if (1u == p_sll->len)
    {
        (p_ht->len)++;
    }

cleanup:
    if (NULL != p_new)
    {
        free(p_new->p_key);
        p_new->p_key = NULL;

        free(p_new->p_value);
        p_new->p_value = NULL;

        free(p_new);
        p_new = NULL;
    }

    return status;
}

status_t
ht_del (ht_t * p_ht, void const * p_key, size_t key_size)
{
    status_t status = STATUS_SUCCESS;

    if (
        (NULL == p_ht) ||
        (NULL == p_key) ||
        (NULL == p_ht->p_hash_func)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    item_t probe =
    {
        .p_ht       = p_ht,
        .hash       = (p_ht->p_hash_func)(p_key, key_size),
        .p_key      = (void *)p_key,
        .key_size   = key_size,
        .p_value    = NULL,
        .value_size = 0u,
    };

    item_t * p_probe = &probe;

    sll_t * p_sll = ht_select(p_ht, &probe);
    status = sll_remove(p_sll, &p_probe, sizeof(p_probe));
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    // Decrement length if last node removed
    if (0u == p_sll->len)
    {
        (p_ht->len)--;
    }

cleanup:
    return status;
}

status_t
ht_foreach (ht_t * p_ht, ht_func_t p_func, void * p_ctx)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_ht) || (NULL == p_func) || (NULL == p_ht->pp_buckets))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    ht_ctx_t ctx =
    {
        .p_func = p_func,
        .p_ctx  = p_ctx,
    };

    for (size_t idx = 0u; idx < p_ht->capacity; idx++)
    {
        sll_t * p_sll = (p_ht->pp_buckets)[idx];
        if (NULL == p_sll)
        {
            continue;
        }

        status = sll_foreach(p_sll, item_foreach, &ctx);
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }
    }

cleanup:
    return status;
}

static uint64_t
djb2_hash (void const * p_key, size_t key_size)
{
    uint64_t hash = 5381u;

    DEBUG_PRINT("Current key: ");
    for (size_t idx = 0u; idx < key_size; idx++)
    {
        uint8_t chr = ((uint8_t *)p_key)[idx];
        DEBUG_PRINT("%c", chr);
        hash = ((hash << 5u) + hash) + chr; // (hash * 33) + chr
    }
    DEBUG_PRINT("\n");

    return hash;
}

static sll_t *
ht_select (ht_t * p_ht, item_t * p_item)
{
    sll_t * p_sll = NULL;

    if ((NULL == p_ht) || (NULL == p_item))
    {
        goto cleanup;
    }

    p_sll = (p_ht->pp_buckets)[p_item->hash % p_ht->capacity];

cleanup:
    return p_sll;
}

static status_t
item_display (void const * p_data)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_data)
    {
        goto cleanup;
    }

    item_t * p_item = (item_t *)p_data;

    // NOTE: Assuming p_key and p_value are strings
    printf(
        "\"%.*s\": \"%.*s\"",
        (int   )(p_item->key_size),
        (char *)(p_item->p_key),
        (int   )(p_item->value_size),
        (char *)(p_item->p_value)
    );

cleanup:
    return status;
}

static int
item_compare (
    void const * p_data1,
    void const * p_data2,
    size_t size
)
{
    int result = 0;
    UNUSED(size);

    item_t * p_item1 = (item_t *)p_data1;
    item_t * p_item2 = (item_t *)p_data2;

    if ((NULL == p_item1) && (NULL == p_item2))
    {
        goto cleanup;
    }

    if (NULL == p_item1)
    {
        result = -1;
        goto cleanup;
    }

    if (NULL == p_item2)
    {
        result = 1;
        goto cleanup;
    }

    if ((NULL == p_item1->p_key) && (NULL == p_item2->p_key))
    {
        goto cleanup;
    }

    if (NULL == p_item1->p_key)
    {
        result = -1;
        goto cleanup;
    }

    if (NULL == p_item2->p_key)
    {
        result = 1;
        goto cleanup;
    }

    if (p_item1->hash != p_item2->hash)
    {
        result = (p_item1->hash > p_item2->hash) ? 1 : -1;
        goto cleanup;
    }

    if (p_item1->key_size != p_item2->key_size)
    {
        result = (p_item1->key_size > p_item2->key_size) ? 1 : -1;
        goto cleanup;
    }

    result = memcmp(p_item1->p_key, p_item2->p_key, p_item1->key_size);

cleanup:
    return result;
}

static void
item_destroy (void * p_data)
{
    item_t * p_item = p_data;

    if (NULL == p_item)
    {
        goto cleanup;
    }

    if ((NULL != p_item->p_ht) && (NULL != p_item->p_ht->p_destroy_key))
    {
        (p_item->p_ht->p_destroy_key)(*(void **)(p_item->p_key));
    }

    free(p_item->p_key);
    p_item->p_key = NULL;

    if ((NULL != p_item->p_ht) && (NULL != p_item->p_ht->p_destroy_value))
    {
        (p_item->p_ht->p_destroy_value)(*(void **)(p_item->p_value));
    }

    free(p_item->p_value);
    p_item->p_value = NULL;

cleanup:
    free(p_item);
    p_item = NULL;
    return;
}

static status_t
item_foreach (node_t * p_node, void * p_ctx)
{
    status_t status = STATUS_SUCCESS;

    ht_ctx_t * p_ht_ctx = p_ctx;

    if (
        (NULL == p_node) ||
        (NULL == p_node->p_data) ||
        (NULL == p_ht_ctx) ||
        (NULL == p_ht_ctx->p_func))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    item_t * p_item = *(void **)(p_node->p_data);

    status = (p_ht_ctx->p_func)(p_item, p_ht_ctx->p_ctx);

cleanup:
    return status;
}

/*** end of file ***/
