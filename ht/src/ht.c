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

/*!
 * @brief Display item
 *
 * @param[in] p_data Pointer to item
 *
 * @return void
 */
static void display_item(void * p_data);

/*!
 * @brief Compare item
 *
 * @param[in] p_data   Pointer to item
 * @param[in] p_key    Pointer to key to compare
 * @param[in] key_size Size of key in bytes
 *
 * @return void
 */
static bool cmp_item(void * p_data, void * p_key, size_t key_size);

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
    p_ht->p_hash = djb2_hash;
    p_ht->p_display_item = display_item;
    p_ht->p_cmp_item = cmp_item;

    p_ht->pp_buckets = calloc(capacity, sizeof(*(p_ht->pp_buckets)));
    if (NULL == p_ht->pp_buckets)
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
        p_sll->p_display_node = p_ht->p_display_item;
        p_sll->p_cmp_node = p_ht->p_cmp_item;

        // Set bucket to empty SLL
        (p_ht->pp_buckets)[idx] = p_sll;
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
    printf("}");

    goto cleanup;

cleanup:
    printf("\n");
    return status;
}

item_t *
ht_get (ht_t * p_ht, void * p_key, size_t key_size)
{
    item_t * p_item = NULL;

    if ((NULL == p_ht) || (NULL == p_key))
    {
        goto cleanup;
    }

    sll_t * p_sll = ht_select(p_ht, p_key, key_size);
    node_t * p_node = sll_get(p_sll, p_key, key_size);

    if (NULL == p_node)
    {
        goto cleanup;
    }

    p_item = p_node->p_data;

    goto cleanup;

cleanup:
    return p_item;
}

status_t
ht_set (ht_t * p_ht,
           void * p_key, size_t key_size,
           void * p_value, size_t value_size)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_ht) || (NULL == p_key) || (NULL == p_value))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sll_t * p_sll = ht_select(p_ht, p_key, key_size);
    node_t * p_node = sll_get(p_sll, p_key, key_size);

    // Update item if key exists in SLL
    if (NULL != p_node)
    {
        // NOTE: Key exists in SLL
        item_t * p_item = p_node->p_data;
        p_item->p_value = p_value;
        p_item->value_size = value_size;

        status = STATUS_EXISTS;
        goto cleanup;
    }

    // Append item if key does not exist in SLL
    item_t item =
    {
        .p_hash     = p_ht->p_hash,
        .hash       = (p_ht->p_hash)(p_key, key_size),
        .p_key      = p_key,
        .key_size   = key_size,
        .p_value    = p_value,
        .value_size = value_size,
    };
    status = sll_append(p_sll, &item, sizeof(item));

    // Increment length if first node inserted
    if ((STATUS_SUCCESS == status) && (1u == p_sll->len))
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
ht_del (ht_t * p_ht, void * p_key, size_t key_size)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_ht) || (NULL == p_key))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sll_t * p_sll = ht_select(p_ht, p_key, key_size);
    node_t * p_node = sll_get(p_sll, p_key, key_size);

    // Remove item if key exists in SLL
    if (NULL == p_node)
    {
        // NOTE: Key does not exist in SLL
        status = STATUS_NOT_EXISTS;
        goto cleanup;
    }

    status = sll_remove(p_sll, p_key, key_size);

    // Decrement length if last node removed
    if ((STATUS_SUCCESS == status) && (0u == p_sll->len))
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
    p_ht->p_hash = NULL;
    p_ht->p_display_item = NULL;
    p_ht->p_cmp_item = NULL;

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

        sll_destroy(p_sll);
        free(p_sll);
        p_sll = NULL;
    }

    p_ht->capacity = 0u;

    // Free all buckets
    free(p_ht->pp_buckets);
    p_ht->pp_buckets = NULL;

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

    uint64_t hash = (p_ht->p_hash)(p_key, key_size);
    p_sll = (p_ht->pp_buckets)[hash % p_ht->capacity];

    goto cleanup;

cleanup:
    return p_sll;
}

static void
display_item (void * p_data)
{
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

    goto cleanup;

cleanup:
    return;
}

static bool
cmp_item (void * p_data, void * p_key, size_t key_size)
{
    bool b_result = false;

    if ((NULL == p_data) || (NULL == p_key))
    {
        goto cleanup;
    }

    item_t * p_item = p_data;
    if ((NULL == p_item->p_key) || (NULL == p_item->p_hash))
    {
        goto cleanup;
    }

    b_result = (p_item->hash == (p_item->p_hash)(p_key, key_size)) &&
               (p_item->key_size == key_size) &&
               (0 == memcmp(p_item->p_key, p_key, key_size));

    goto cleanup;

cleanup:
    return b_result;
}

/*** end of file ***/
