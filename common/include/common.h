/** @file common.h
 *
 * @brief Common definitions header
 *
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
    #define DEBUG_PRINT(...) (fprintf(stderr, __VA_ARGS__))
#else
    #define DEBUG_PRINT(...)
#endif

#define UNUSED(var) ((void)(var))

typedef void (*display_func_t)(void * p_data);
typedef bool (*cmp_func_t)(void * p_data, void * p_data2);

typedef enum status
{
    STATUS_SUCCESS           = EXIT_SUCCESS,
    STATUS_FAILURE           = EXIT_FAILURE,
    STATUS_NULL_ARG          = 2,
    STATUS_ALLOC_FAILURE     = 3,
    STATUS_SIGNAL_FAILURE    = 4,
    STATUS_MUTEX_FAILURE     = 5,
    STATUS_SOCKET_FAILURE    = 6,
    STATUS_SEND_FAILURE      = 7,
    STATUS_RECV_FAILURE      = 8,
    STATUS_CLIENT_DISCONNECT = 9,
    STATUS_SERVER_DISCONNECT = 10,
    STATUS_OVERFLOW          = 11,
    STATUS_UNDERFLOW         = 12,
    STATUS_EMPTY             = 13,
    STATUS_EXISTS            = 14,
    STATUS_NOT_EXISTS        = 15,
    STATUS_OUT_OF_BOUNDS     = 16,
} status_t;

/*!
 * @brief Display variable bytes in hex
 *
 * @param[in] p_var Pointer to variable
 * @param[in] size  Size of variable in bytes
 * @param[in] p_sep Pointer to separator between each byte
 *
 * @return void
 */
void display_bytes(void * p_var, size_t size, char const * p_sep);

#endif /* COMMON_H */

/*** end of file ***/
