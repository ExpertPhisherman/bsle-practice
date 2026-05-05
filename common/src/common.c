/** @file common.c
 *
 * @brief Common definitions source
 *
 * @par
 *
 */

#include "common.h"

status_t
display_hex (void * p_var, size_t size, char const * p_sep)
{
    status_t status = STATUS_SUCCESS;

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

    return status;
}

status_t
display_printable (char * p_buf, size_t size)
{
    status_t status = STATUS_SUCCESS;

    uint8_t chr;

    for (size_t idx = 0u; idx < size; idx++)
    {
        chr = ((uint8_t *)p_buf)[idx];
        if (isprint(chr))
        {
            printf("%c", chr);
        }
    }

    return status;
}

/*** end of file ***/
