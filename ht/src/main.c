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

    ht_create(&ht, 17u);

    char const * key_arr[] =
    {
        "invinate", "blighter", "screeman", "pisachee", "tethelin", "medullar",
        "overdoer", "conjurer", "tunicate", "stratous", "inextant", "sombrous",
        "tenorite", "perflate", "narrater", "linolate", "wolfskin", "unrotted",
        "remargin", "dragbolt", "allosome", "ratproof", "unshaken", "ditroite",
        "reducent", "distance", "snowbird", "vitalist", "expirant", "femality",
        "perscent", "slapping", "histonal", "analytic", "belltail", "centrist"
    };

    size_t len = sizeof(key_arr) / sizeof(*key_arr);
    for (size_t idx = 0u; idx < len; idx++)
    {
        char const * key = key_arr[idx];
        size_t size = strnlen(key, 256u);
        ht_insert(&ht, (void *)key, size);
    }

    ht_display(&ht);

    printf("Offset: %zu\n", offsetof(ht_t, pp_elements));
    printf("Offset: %zu\n", offsetof(ht_t, capacity));
    printf("Offset: %zu\n", offsetof(ht_t, len));
    printf("Offset: %zu\n", offsetof(ht_t, p_hash_func));

    goto cleanup;

cleanup:
    ht_destroy(&ht);

    (void)argc;
    (void)argv;

    return status;
}

/*** end of file ***/
