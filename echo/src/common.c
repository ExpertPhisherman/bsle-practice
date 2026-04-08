/** @file common.h
 *
 * @brief Common definitions source
 *
 */

#include "common.h"

void
display_bytes (void * p_var, size_t size, char const * p_sep)
{
    uint8_t chr;

    for (size_t index = 0u; index < size; index++)
    {
        chr = ((uint8_t *)p_var)[index];
        printf("%02hhx", chr);
        if (1u < (size - index))
        {
            printf("%s", p_sep);
        }
    }
}

void
nfree (void ** pp_var)
{
    if (NULL == pp_var)
    {
        goto cleanup;
    }

    if (NULL == *pp_var)
    {
        goto cleanup;
    }

    free(*pp_var);
    *pp_var = NULL;

cleanup:
    return;
}

/*** end of file ***/
