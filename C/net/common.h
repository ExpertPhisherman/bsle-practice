/** @file common.h
 *
 * @brief Common definitions
 *
 */

#ifndef COMMON_H
#define COMMON_H

typedef enum
{
    SUCCESS,
    NULL_ARG,
    ALLOC_FAILURE,
    OVERFLOW,
    UNDERFLOW,
    EMPTY,
    EXISTS,
    NOT_EXISTS
} status_t;

#endif /* COMMON_H */

/*** end of file ***/
