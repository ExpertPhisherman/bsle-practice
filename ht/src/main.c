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
    for (size_t idx = 0u; idx < len; idx++)
    {
        char const * data = data_arr[idx];
        size_t size = strnlen(data, 256u);
        ht_insert(&ht, (void *)data, size);
    }

    ht_display(&ht);

    goto cleanup;

cleanup:
    ht_destroy(&ht);

    (void)argc;
    (void)argv;

    return status;
}

/*** end of file ***/
