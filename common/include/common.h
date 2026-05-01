/** @file common.h
 *
 * @brief Common definitions header
 *
 * @par
 *
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#ifdef DEBUG
    #define DEBUG_PRINT(...) (fprintf(stderr, __VA_ARGS__))
#else
    #define DEBUG_PRINT(...)
#endif

#define UNUSED(var) ((void)(var))

#define MUTEX_CALL(p_func, lock, ...)            \
({                                               \
    pthread_mutex_lock(&(lock));                 \
    __auto_type _result = (p_func)(__VA_ARGS__); \
    pthread_mutex_unlock(&(lock));               \
    _result;                                     \
})

typedef enum status
{
    STATUS_SUCCESS           = EXIT_SUCCESS,
    STATUS_FAILURE           = EXIT_FAILURE,
    STATUS_NULL_ARG          = 2,
    STATUS_ALLOC_FAILURE     = 3,
    STATUS_SIGNAL_FAILURE    = 4,
    STATUS_THREAD_FAILURE    = 5,
    STATUS_MUTEX_FAILURE     = 6,
    STATUS_SOCKET_FAILURE    = 7,
    STATUS_SEND_FAILURE      = 8,
    STATUS_RECV_FAILURE      = 9,
    STATUS_CLIENT_DISCONNECT = 10,
    STATUS_SERVER_DISCONNECT = 11,
    STATUS_OVERFLOW          = 12,
    STATUS_UNDERFLOW         = 13,
    STATUS_EMPTY             = 14,
    STATUS_FULL              = 15,
    STATUS_EXISTS            = 16,
    STATUS_NOT_EXISTS        = 17,
    STATUS_OUT_OF_BOUNDS     = 18,
    STATUS_SIZE_MISMATCH     = 19,
} status_t;

typedef status_t (*display_func_t)(void * p_data);
typedef bool     (*cmp_func_t)(void * p_data1, void * p_data2);

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
