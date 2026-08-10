/** @file main.c
 *
 * @brief Main source
 *
 * @par
 *
 */

 #include <stddef.h>

#include "main.h"
#include "common.h"
#include "bst.h"

int
main (int argc, char * pp_argv[])
{
    status_t status = STATUS_SUCCESS;
    UNUSED(argc);
    UNUSED(pp_argv);

    bst_t * p_bst = bst_create();
    if (NULL == p_bst)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

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
        char const * p_key = p_keys[idx];
        size_t key_size = strnlen(p_key, 256u);
        bst_set(p_bst, p_key, key_size);
    }

    bst_set(p_bst, "obama", 5u);
    bst_set(p_bst, "obama", 5u);
    bst_set(p_bst, "dragbolt", 8u);
    bst_del(p_bst, "femality", 8u);

    bst_display(p_bst, ", ");

    bst_destroy(p_bst);
    p_bst = NULL;

    goto cleanup;

cleanup:
    return status;
}

/*** end of file ***/
