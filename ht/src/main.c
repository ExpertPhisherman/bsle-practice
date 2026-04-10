/** @file main.c
 *
 * @brief Main source
 *
 */

#include "../include/main.h"

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;
    ht_t ht;
    size_t idx;
    sll_t sll;
    bool b_data_in;

    const char * data;

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
    char const * data_arr[] =
    {
        "invinate", "blighter", "screeman", "pisachee", "tethelin", "medullar",
        "overdoer", "conjurer", "tunicate", "stratous", "inextant", "sombrous",
        "tenorite", "perflate", "narrater", "linolate", "wolfskin", "unrotted",
        "remargin", "dragbolt", "allosome", "ratproof", "unshaken", "ditroite",
        "reducent", "distance", "snowbird", "vitalist", "expirant", "femality",
        "perscent", "slapping", "histonal", "analytic", "belltail", "centrist"
    };
    size_t len = sizeof(data_arr) / sizeof(char *);
    for (idx = 0u; idx < len; idx++)
    {
        data = data_arr[idx];
        size_t size = strnlen(data, 256u);
        status = ht_insert(&ht, (void *)data, size);
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
    idx = 13u;
    status = ht_get(&ht, idx, &sll);
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
        printf("%zu: ", idx);
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
    size_t size = strnlen(data, 256u);
    status = ht_remove(&ht, (void *)data, size);
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
    b_data_in = ht_in(&ht, (void *)data, size);
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
    printf("Length: %zu\n", ht.len);

cleanup:
    ht_destroy(&ht);

    (void)argc;
    (void)argv;

    return status;
}

/*** end of file ***/
