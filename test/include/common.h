/** @file common.h
 *
 * @brief Common definitions header
 *
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum status
{
    STATUS_SUCCESS           = EXIT_SUCCESS,
    STATUS_FAILURE           = EXIT_FAILURE,
    STATUS_NULL_ARG          = 2,
    STATUS_SIGNAL_FAILURE    = 3,
    STATUS_SOCKET_FAILURE    = 4,
    STATUS_SEND_FAILURE      = 5,
    STATUS_RECV_FAILURE      = 6,
    STATUS_CLIENT_DISCONNECT = 7,
    STATUS_SERVER_DISCONNECT = 8,
    STATUS_ALLOC_FAILURE     = 9,
    STATUS_OVERFLOW          = 10,
    STATUS_UNDERFLOW         = 11,
    STATUS_EMPTY             = 12,
    STATUS_EXISTS            = 13,
    STATUS_NOT_EXISTS        = 14,
    STATUS_OUT_OF_BOUNDS     = 15
} status_t;

/*!
 * @brief Display variable bytes in hex
 *
 * @param[in] p_var Pointer to variable
 * @param[in] size  Variable size in bytes
 * @param[in] p_sep Pointer to separator between each byte
 *
 * @return void
 */
void display_bytes(void const * const p_var, size_t const size, char const * const p_sep);

/*!
 * @brief Free and set argument to NULL
 *
 * @param[in] pp_var Double pointer to variable
 *
 * @return void
 */
void nfree(void const ** const pp_var);

#endif /* COMMON_H */

/*** end of file ***/
