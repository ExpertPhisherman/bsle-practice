/** @file common.h
 *
 * @brief Common definitions
 *
 */

#ifndef COMMON_H
#define COMMON_H

typedef enum
{
    STATUS_SUCCESS       = 0,
    STATUS_NULL_ARG      = 1,
    STATUS_ALLOC_FAILURE = 2,
    STATUS_OVERFLOW      = 3,
    STATUS_UNDERFLOW     = 4,
    STATUS_EMPTY         = 5,
    STATUS_EXISTS        = 6,
    STATUS_NOT_EXISTS    = 7
} status_t;

#endif /* COMMON_H */

/*** end of file ***/
