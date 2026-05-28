/** @file main.c
 *
 * @brief Main source
 *
 * @par
 *
 */

#include "main.h"

int
main (int argc, char * argv[])
{
    status_t status = STATUS_SUCCESS;

    ht_t * p_ht = ht_create(17u);

    char const * p_keys[] =
    {
        "invinate", "blighter", "screeman", "pisachee", "tethelin", "medullar",
        "overdoer", "conjurer", "tunicate", "stratous", "inextant", "sombrous",
        "tenorite", "perflate", "narrater", "linolate", "wolfskin", "unrotted",
        "remargin", "dragbolt", "allosome", "ratproof", "unshaken", "ditroite",
        "reducent", "distance", "snowbird", "vitalist", "expirant", "femality",
        "perscent", "slapping", "histonal", "analytic", "belltail", "centrist",
    };

    size_t len = sizeof(p_keys) / sizeof(*p_keys);
    for (size_t idx = 0u; idx < len; idx++)
    {
        char const * key = p_keys[idx];
        size_t key_size = strnlen(key, 256u);
        ht_set(p_ht, key, key_size, key, 4u);
    }

    item_t * p_item = NULL;

    ht_set(p_ht, "obama", 5u, "TEST", 4u);
    ht_set(p_ht, "obama", 5u, "pyramid", 7u);
    ht_set(p_ht, "dragbolt", 8u, "pluh", 4u);
    ht_del(p_ht, "femality", 8u);

    p_item = ht_get(p_ht, "obama", 5u);
    (p_ht->p_display_item)(p_item);
    printf("\n");

    p_item = ht_get(p_ht, "dragbolt", 8u);
    (p_ht->p_display_item)(p_item);
    printf("\n");

    ht_display(p_ht, ", ");

    ht_destroy(p_ht);
    p_ht = NULL;

    goto cleanup;

cleanup:
    UNUSED(argc);
    UNUSED(argv);

    return status;
}

/*** end of file ***/
