/** @file test.c
 *
 * @brief A description of the module's purpose.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2018 Barr Group. All rights reserved.
 */

#include "test.h"

/*!
 * @brief Identify the endianness of the system.
 *
 * @return true for little-endian, false for big-endian.
 */
bool
endianness (void)
{
    uint_fast16_t one = 1u;
    char * p_first_byte = (char*)&one;
    return (1 == (int)*p_first_byte);
}

int
main (int argc, char * argv[])
{
    bool b_little_endian = endianness();
    printf("%d\n", b_little_endian);
    return 0;
}

/*** end of file ***/
