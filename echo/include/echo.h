/** @file echo.h
 *
 * @brief Echo server
 *
 * @par
 *
 */

#ifndef ECHO_H
#define ECHO_H

#include "server.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef enum opcode
{
    OPCODE_PING = 0x01, // Respond with PONG
    OPCODE_ECHO = 0x02, // Return the provided message
    OPCODE_QUIT = 0x03, // Close client connection
} opcode_t;

/*!
 * @brief Load application functions
 *
 * @param[in] p_server Pointer to server
 *
 * @return Status of operation
 */
status_t load_app(server_t * p_server);

#endif /* ECHO_H */

/*** end of file ***/
