/** @file main.c
 *
 * @brief Main source
 *
 * @par
 *
 */

#include "../include/main.h"

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;

    ht_t ht;

    ht_create(&ht, 17u);

    char const * key_arr[] =
    {
        "invinate", "blighter", "screeman", "pisachee", "tethelin", "medullar",
        "overdoer", "conjurer", "tunicate", "stratous", "inextant", "sombrous",
        "tenorite", "perflate", "narrater", "linolate", "wolfskin", "unrotted",
        "remargin", "dragbolt", "allosome", "ratproof", "unshaken", "ditroite",
        "reducent", "distance", "snowbird", "vitalist", "expirant", "femality",
        "perscent", "slapping", "histonal", "analytic", "belltail", "centrist",
    };

    size_t len = sizeof(key_arr) / sizeof(*key_arr);
    for (size_t idx = 0u; idx < len; idx++)
    {
        char const * key = key_arr[idx];
        size_t key_size = strnlen(key, 256u);
        ht_set(&ht, (void *)key, key_size, (void *)key, 4u);
    }

    item_t * p_item = NULL;

    ht_set(&ht, &(int){97}, 4u, (void *)"TEST", 4u);
    ht_set(&ht, (void *)"dragbolt", 8u, (void *)"pluh", 4u);
    ht_del(&ht, (void *)"femality", 8u);

    p_item = ht_get(&ht, &(int){97}, 4u);
    (ht.p_display_item)(p_item);
    printf("\n");

    p_item = ht_get(&ht, (void *)"dragbolt", 8u);
    (ht.p_display_item)(p_item);
    printf("\n");

    ht_display(&ht, ", ");

    ht_destroy(&ht);

    goto cleanup;

cleanup:
    UNUSED(argc);
    UNUSED(argv);

    return status;
}

/*** end of file ***/
