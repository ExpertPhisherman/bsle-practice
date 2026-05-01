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
static status_t display_item(void * p_data);

/*!
 * @brief Compare items
 *
 * @param[in] p_data1 Pointer to first item
 * @param[in] p_data2 Pointer to second item
 *
 * @return void
 */
static bool cmp_item(void * p_data1, void * p_data2);

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

    p_ht = malloc(sizeof(*p_ht));
    if (NULL == p_ht)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_ht->capacity       = capacity;
    p_ht->len            = 0u;
    p_ht->p_hash_func    = djb2_hash;
    p_ht->p_display_item = display_item;
    p_ht->p_cmp_item     = cmp_item;

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

        p_sll->p_display_node = p_ht->p_display_item;
        p_sll->p_cmp_node = p_ht->p_cmp_item;

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

    printf("{");
    bool b_first = true;
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
ht_get (ht_t * p_ht, void * p_key, size_t key_size)
{
    item_t * p_item = NULL;

    if ((NULL == p_ht) || (NULL == p_key) || (NULL == p_ht->p_hash_func))
    {
        goto cleanup;
    }

    item_t item =
    {
        .p_hash_func = p_ht->p_hash_func,
        .hash        = (p_ht->p_hash_func)(p_key, key_size),
        .p_key       = p_key,
        .key_size    = key_size,
    };

    sll_t * p_sll = ht_select(p_ht, &item);
    node_t * p_node = sll_get(p_sll, &item);

    if (NULL == p_node)
    {
        goto cleanup;
    }

    p_item = p_node->p_data;

cleanup:
    return p_item;
}

status_t
ht_set (ht_t * p_ht,
        void * p_key, size_t key_size,
        void * p_value, size_t value_size)
{
    status_t status = STATUS_SUCCESS;

    item_t item =
    {
        .p_hash_func = p_ht->p_hash_func,
        .hash        = (p_ht->p_hash_func)(p_key, key_size),
        .p_key       = NULL,
        .key_size    = key_size,
        .p_value     = NULL,
        .value_size  = value_size,
    };

    if ((NULL == p_ht) ||
        (NULL == p_key) || (NULL == p_value) ||
        (NULL == p_ht->p_hash_func))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Allocate hash table owned key and value
    item.p_key = malloc(key_size);
    if (NULL == item.p_key)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    memcpy(item.p_key, p_key, key_size);

    item.p_value = malloc(value_size);
    if (NULL == item.p_value)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    memcpy(item.p_value, p_value, value_size);

    sll_t * p_sll = ht_select(p_ht, &item);
    node_t * p_node = sll_get(p_sll, &item);

    // Update item if key exists in SLL
    if (NULL != p_node)
    {
        // NOTE: Key exists in SLL
        item_t * p_item = p_node->p_data;

        // Free old value
        free(p_item->p_value);
        p_item->p_value = NULL;

        p_item->p_value    = item.p_value;
        p_item->value_size = item.value_size;

        // Free key copy
        free(item.p_key);
        item.p_key = NULL;

        status = STATUS_EXISTS;
        goto cleanup;
    }

    // Append item if key does not exist in SLL
    status = sll_append(p_sll, &item, sizeof(item));

    // Increment length if first node inserted
    if ((STATUS_SUCCESS == status) && (1u == p_sll->len))
    {
        (p_ht->len)++;
    }

cleanup:
    if (STATUS_ALLOC_FAILURE == status)
    {
        free(item.p_key);
        item.p_key = NULL;

        free(item.p_value);
        item.p_value = NULL;

        ht_destroy(p_ht);
        p_ht = NULL;
    }

    return status;
}

status_t
ht_del (ht_t * p_ht, void * p_key, size_t key_size)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_ht) || (NULL == p_key) || (NULL == p_ht->p_hash_func))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    item_t item =
    {
        .p_hash_func = p_ht->p_hash_func,
        .hash        = (p_ht->p_hash_func)(p_key, key_size),
        .p_key       = p_key,
        .key_size    = key_size,
    };

    sll_t * p_sll = ht_select(p_ht, &item);
    node_t * p_node = sll_get(p_sll, &item);

    // Remove item if key exists in SLL
    if (NULL == p_node)
    {
        // NOTE: Key does not exist in SLL
        status = STATUS_NOT_EXISTS;
        goto cleanup;
    }

    // Free heap allocated key and value
    item_t * p_item = p_node->p_data;
    void * p_key_cpy = p_item->p_key;
    void * p_val_cpy = p_item->p_value;

    status = sll_remove(p_sll, &item);

    if (STATUS_SUCCESS == status)
    {
        free(p_key_cpy);
        p_key_cpy = NULL;
        free(p_val_cpy);
        p_val_cpy = NULL;

        // Decrement length if last node removed
        if (0u == p_sll->len)
        {
            (p_ht->len)--;
        }
    }

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

    p_ht->len            = 0u;
    p_ht->p_hash_func    = NULL;
    p_ht->p_display_item = NULL;
    p_ht->p_cmp_item     = NULL;

    if (NULL == p_ht->pp_buckets)
    {
        p_ht->capacity = 0u;
        goto cleanup;
    }

    // Free each bucket
    for (size_t idx = 0u; idx < p_ht->capacity; idx++)
    {
        sll_t * p_sll = (p_ht->pp_buckets)[idx];
        (p_ht->pp_buckets)[idx] = NULL;

        if (NULL != p_sll)
        {
            // Free heap allocated key and value
            node_t * p_curr = p_sll->p_head;
            while (NULL != p_curr)
            {
                item_t * p_item = p_curr->p_data;
                if (NULL != p_item)
                {
                    free(p_item->p_key);
                    p_item->p_key = NULL;
                    free(p_item->p_value);
                    p_item->p_value = NULL;
                }
                p_curr = p_curr->p_next;
            }
        }

        sll_destroy(p_sll);
        p_sll = NULL;
    }

    p_ht->capacity = 0u;

    free(p_ht->pp_buckets);
    p_ht->pp_buckets = NULL;

cleanup:
    free(p_ht);
    p_ht = NULL;
    return status;
}

static uint64_t
djb2_hash (void * p_key, size_t key_size)
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
display_item (void * p_data)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_data)
    {
        goto cleanup;
    }

    item_t * p_item = p_data;

    printf("\"");
    // NOTE: Assuming p_key is a string
    printf("%.*s", (int)(p_item->key_size), (char *)(p_item->p_key));
    printf("\": \"");
    // NOTE: Assuming p_value is a string
    printf("%.*s", (int)(p_item->value_size), (char *)(p_item->p_value));
    printf("\"");

cleanup:
    return status;
}

static bool
cmp_item (void * p_data1, void * p_data2)
{
    bool b_result = false;

    if ((NULL == p_data1) || (NULL == p_data2))
    {
        goto cleanup;
    }

    item_t * p_item1 = p_data1;
    if (NULL == p_item1->p_key)
    {
        goto cleanup;
    }

    item_t * p_item2 = p_data2;
    if (NULL == p_item2->p_key)
    {
        goto cleanup;
    }

    b_result = (p_item1->hash == p_item2->hash) &&
               (p_item1->key_size == p_item2->key_size) &&
               (0 == memcmp(p_item1->p_key, p_item2->p_key, p_item1->key_size));

cleanup:
    return b_result;
}

/*** end of file ***/
