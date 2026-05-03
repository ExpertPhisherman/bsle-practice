/** @file common.c
 *
 * @brief Common definitions source
 *
 * @par
 *
 */

#include "common.h"

void
display_bytes (void * p_var, size_t size, char const * p_sep)
{
    uint8_t chr;

    for (size_t idx = 0u; idx < size; idx++)
    {
        // Print separator between each byte
        if (1u <= idx)
        {
            printf("%s", p_sep);
        }

        chr = ((uint8_t *)p_var)[idx];
        printf("%02hhx", chr);
    }
}

/*** end of file ***/
