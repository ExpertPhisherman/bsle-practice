/** @file sockutil.h
 *
 * @brief Socket utility header
 *
 * @par
 *
 */

#ifndef SOCKUTIL_H
#define SOCKUTIL_H

#include "common.h"
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/*!
 * @brief Send all data to socket
 *
 * @param[in] sockfd Socket to send data to
 * @param[in] p_buf  Buffer to send
 * @param[in] size   Number of bytes to send
 *
 * @return Status of operation
 */
status_t sockutil_sendall(int sockfd, void * p_buf, size_t size);

/*!
 * @brief Receive all data from socket
 *
 * @param[in] sockfd Socket to receive data from
 * @param[in] p_buf  Buffer to receive into
 * @param[in] size   Number of bytes to receive
 *
 * @return Status of operation
 */
status_t sockutil_recvall(int sockfd, void * p_buf, size_t size);

/*!
 * @brief Drain bytes from socket
 *
 * @param[in] sockfd     Socket file descriptor
 * @param[in] size       Number of bytes to drain
 * @param[in] chunk_size Number of bytes per chunk
 *
 * @return Status of operation
 */
status_t sockutil_drain(int sockfd, size_t size, size_t chunk_size);

#endif /* SOCKUTIL_H */

/*** end of file ***/
