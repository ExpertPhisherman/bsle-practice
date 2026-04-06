/** @file common.h
 *
 * @brief Common definitions
 *
 */

#include "common.h"

void
display_bytes (void const * const p_var, size_t const size, char const * const p_sep)
{
    uint8_t chr;

    for (size_t index = 0u; index < size; index++)
    {
        chr = ((uint8_t*)p_var)[index];
        printf("%02hhx", chr);
        if (1u < (size - index))
        {
            printf("%s", p_sep);
        }
    }
}

/*** end of file ***/
