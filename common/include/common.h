/** @file common.h
 *
 * @brief Common definitions header
 *
 * @par
 *
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>

#ifdef DEBUG
#   define DEBUG_PRINT(...) (fprintf(stderr, ##__VA_ARGS__))
#else
#   define DEBUG_PRINT(...) ((void)0)
#endif

#define UNUSED(var) ((void)(var))

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
    STATUS_INVALID_SESSION   = 20,
} status_t;

typedef int (*compare_func_t)(
    void const * p_data1,
    void const * p_data2,
    size_t size
);

typedef status_t (*display_func_t)(void const * p_data);
typedef void     (*destroy_func_t)(void * p_data);
typedef int      (*ischartype_func_t)(int chr);

/*!
 * @brief Check if all characters in string are of character type
 *
 * @param[in] p_str  Pointer to string
 * @param[in] size   Size of string in bytes
 * @param[in] p_func Pointer to character type function
 *
 * @return Status of operation
 */
int ischartype_str(
    char const        * p_str,
    size_t              size,
    ischartype_func_t   p_func
);

/*!
 * @brief Display bytes in hex
 *
 * @param[in] p_buf Pointer to buffer
 * @param[in] size  Size of buffer in bytes
 * @param[in] p_sep String inserted between each byte
 * @param[in] p_end String appended after last byte
 *
 * @return Status of operation
 */
status_t display_hex(
    void const * p_buf,
    size_t       size,
    char const * p_sep,
    char const * p_end
);

/*!
 * @brief Display printable characters
 *
 * @param[in] p_buf Pointer to buffer
 * @param[in] size  Size of buffer in bytes
 * @param[in] p_sep String inserted between each byte
 * @param[in] p_end String appended after last byte
 *
 * @return Status of operation
 */
status_t display_printable(
    void const * p_buf,
    size_t       size,
    char const * p_sep,
    char const * p_end
);

/*!
 * @brief Display unicode characters
 *
 * @par
 * If character is decodable, print decoded character
 * Else, print "."
 *
 * @param[in] p_buf Pointer to buffer
 * @param[in] size  Size of buffer in bytes
 * @param[in] p_sep String inserted between each byte
 * @param[in] p_end String appended after last byte
 *
 * @return Status of operation
 */
status_t display_unicode(
    void const * p_buf,
    size_t       size,
    char const * p_sep,
    char const * p_end
);

/*!
 * @brief Python-like print to stream
 *
 * @par
 * Inspired by Python:
 *
 * $ python3 -m pydoc print | cat
 * Help on built-in function print in module builtins:
 *
 * print(...)
 *     print(value, ..., sep=' ', end='\n', file=sys.stdout, flush=False)
 *
 *     Prints the values to a stream, or to sys.stdout by default.
 *     Optional keyword arguments:
 *     file:  a file-like object (stream); defaults to the current sys.stdout.
 *     sep:   string inserted between values, default a space.
 *     end:   string appended after the last value, default a newline.
 *     flush: whether to forcibly flush the stream.
 *
 * @param[in] p_stream     Pointer to stream to write to
 * @param[in] p_buf        Pointer to buffer to read from
 * @param[in] size         Size of buffer in bytes
 * @param[in] p_sep        String inserted between each byte
 * @param[in] p_end        String appended after last byte
 * @param[in] p_ischartype Pointer to character type check function
 * @param[in] p_fmt_true   String when function returns non-zero
 * @param[in] p_fmt_false  String when function returns zero
 * @param[in] b_flush      Whether to forcibly flush the stream
 *
 * @return Status of operation
 */
status_t fprint(
    FILE              * p_stream,
    void const        * p_buf,
    size_t              size,
    char const        * p_sep,
    char const        * p_end,
    ischartype_func_t   p_ischartype,
    char const        * p_fmt_true,
    char const        * p_fmt_false,
    bool                b_flush
);

#endif /* COMMON_H */

/*** end of file ***/
