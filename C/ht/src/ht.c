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

#include "../include/ht.h"

uint64_t
djb2_hash (void const * const key, size_t const key_len)
{
    unsigned char chr;

    uint64_t hash = 5381u;

    printf("Current key: ");
    for (size_t index = 0u; index < key_len; index++)
    {
        chr = ((unsigned char*)key)[index];
        printf("%c", chr);
        hash = ((hash << 5u) + hash) + chr; // (hash * 33) + chr
    }
    printf("\n");

    return hash;
}

status_t
ht_create (ht_t * p_ht, size_t const capacity)
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
    p_ht->size = 0u;
    p_ht->hash = djb2_hash;

    // Allocate entries
    p_ht->pp_entries = malloc(sizeof(sll_t*) * capacity);
    if (NULL == (p_ht->pp_entries))
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memset(p_ht->pp_entries, 0, sizeof(sll_t*) * capacity);

    // Allocate and set entries to empty SLLs
    for (size_t index = 0u; index < capacity; index++)
    {
        // Allocate entry
        sll_t * p_sll = malloc(sizeof(sll_t));
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
        (p_ht->pp_entries)[index] = p_sll;
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

    if (0u == p_ht->size)
    {
        status = STATUS_EMPTY;
        goto cleanup;
    }

    for (size_t index = 0u; index < (p_ht->capacity); index++)
    {
        sll_t * p_sll = (p_ht->pp_entries)[index];
        if (NULL != (p_sll->p_head))
        {
            printf("%zu: ", index);
            status = sll_display(p_sll);
            if (STATUS_NULL_ARG == status)
            {
                goto cleanup;
            }
        }
    }

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    return status;
}

bool
ht_in (ht_t * p_ht, sll_data_t const data)
{
    bool b_data_in;

    if (NULL == p_ht)
    {
        b_data_in = false;
        goto cleanup;
    }

    size_t const capacity = p_ht->capacity;
    uint64_t hash = ((p_ht->hash)(data, strnlen(data, 256u))) % capacity;

    b_data_in = sll_in((p_ht->pp_entries)[hash], data);
    goto cleanup;

cleanup:
    return b_data_in;
}

status_t
ht_get (ht_t * p_ht, size_t index, sll_t * p_sll)
{
    status_t status;

    if (NULL == p_ht)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (index >= p_ht->capacity)
    {
        status = STATUS_OVERFLOW;
        goto cleanup;
    }

    // Copy SLL into destination variable
    *p_sll = *((p_ht->pp_entries)[index]);

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    return status;
}

status_t
ht_insert (ht_t * p_ht, sll_data_t const data)
{
    status_t status;

    if (NULL == p_ht)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    size_t const capacity = p_ht->capacity;
    uint64_t hash = ((p_ht->hash)(data, strnlen(data, 256u))) % capacity;

    sll_t * p_sll = (p_ht->pp_entries)[hash];

    // Insert data if not already exists in SLL
    if (!sll_in(p_sll, data))
    {
        // Insert node as new head
        status = sll_insert(p_sll, data, 0u);

        // Increment size if first node inserted
        if (1u == (p_sll->size))
        {
            (p_ht->size)++;
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
ht_remove (ht_t * p_ht, sll_data_t const data)
{
    status_t status;

    if (NULL == p_ht)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    size_t const capacity = p_ht->capacity;
    uint64_t hash = ((p_ht->hash)(data, strnlen(data, 256u))) % capacity;

    sll_t * p_sll = (p_ht->pp_entries)[hash];

    // Remove data if exists in SLL
    if (sll_in(p_sll, data))
    {
        status = sll_remove(p_sll, data);

        // Decrement size if SLL is empty
        if (0u == (p_sll->size))
        {
            (p_ht->size)--;
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
    for (size_t index = 0u; index < (p_ht->capacity); index++)
    {
        sll_t * p_sll = (p_ht->pp_entries)[index];

        // Free SLL nodes
        sll_destroy(p_sll);

        // Free SLL
        if (NULL != p_sll)
        {
            free(p_sll);
            p_sll = NULL;
        }
    }

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    // Free all entries
    if (NULL != (p_ht->pp_entries))
    {
        free(p_ht->pp_entries);
        p_ht->pp_entries = NULL;
    }

    p_ht->capacity = 0u;
    p_ht->size = 0u;
    p_ht->hash = NULL;

    return status;
}

int
main (void)
{
    status_t status;
    ht_t ht;
    sll_data_t data;
    size_t index;
    sll_t sll;
    bool b_data_in;

    // Create hash table with sample capacity
    status = ht_create(&ht, 24u);
    if (STATUS_NULL_ARG == status)
    {
        printf("NULL argument is present.\n");
        goto cleanup;
    }
    else if (STATUS_ALLOC_FAILURE == status)
    {
        printf("Failed while allocating memory!\n");
        goto cleanup;
    }
    else if (STATUS_EMPTY == status)
    {
        printf("Cannot create hashtable with 0 capacity!\n");
        goto cleanup;
    }

    // Insert data
    sll_data_t data_arr[] =
    {
        "invinate", "blighter", "screeman", "pisachee", "tethelin", "medullar",
        "overdoer", "conjurer", "tunicate", "stratous", "inextant", "sombrous",
        "tenorite", "perflate", "narrater", "linolate", "wolfskin", "unrotted",
        "remargin", "dragbolt", "allosome", "ratproof", "unshaken", "ditroite",
        "reducent", "distance", "snowbird", "vitalist", "expirant", "femality",
        "perscent", "slapping", "histonal", "analytic", "belltail", "centrist"
    };
    size_t size = sizeof(data_arr) / sizeof(sll_data_t);
    for (index = 0u; index < size; index++)
    {
        data = data_arr[index];
        status = ht_insert(&ht, data);
        if (STATUS_NULL_ARG == status)
        {
            printf("NULL argument is present.\n");
            goto cleanup;
        }
        else if (STATUS_EXISTS == status)
        {
            printf("\"%s\" already exists in SLL. Leaving as is.\n", data);
        }
        else if (STATUS_ALLOC_FAILURE == status)
        {
            printf("Failed while allocating memory!\n");
            goto cleanup;
        }
    }

    // Get nth SLL in hash table
    index = 5u;
    status = ht_get(&ht, index, &sll);
    if (STATUS_NULL_ARG == status)
    {
        printf("NULL argument is present.\n");
        goto cleanup;
    }
    else if (STATUS_OVERFLOW == status)
    {
        printf("Index out of range!\n");
        goto cleanup;
    }
    else
    {
        printf("%zu: ", index);
        status = sll_display(&sll);
        if (STATUS_NULL_ARG == status)
        {
            printf("NULL argument is present.\n");
            goto cleanup;
        }
        else if (STATUS_EMPTY == status)
        {
            printf("SLL is empty.\n");
        }
    }

    // Remove data
    data = "snowbird";
    status = ht_remove(&ht, data);
    if (STATUS_NULL_ARG == status)
    {
        printf("NULL argument is present.\n");
        goto cleanup;
    }
    else if (STATUS_NOT_EXISTS == status)
    {
        printf("\"%s\" not in hash table. Cannot remove.\n", data);
    }
    else if (STATUS_ALLOC_FAILURE == status)
    {
        printf("Failed while allocating memory!\n");
        goto cleanup;
    }
    else
    {
        printf("\"%s\" removed.\n", data);
    }

    // Check data after removal
    b_data_in = ht_in(&ht, data);
    printf("\"%s\" %sin hash table.\n", data, b_data_in ? "" : "not ");

    // Display hash table entries
    status = ht_display(&ht);
    if (STATUS_NULL_ARG == status)
    {
        printf("NULL argument is present.\n");
        goto cleanup;
    }
    else if (STATUS_EMPTY == status)
    {
        printf("Hash table is empty.\n");
    }
    printf("Size: %zu\n", ht.size);

    goto cleanup;

cleanup:
    ht_destroy(&ht);

    return status;
}

/*** end of file ***/
