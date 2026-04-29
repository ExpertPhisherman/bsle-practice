/** @file chat.h
 *
 * @brief Chat server
 *
 * @par
 *
 */

#ifndef CHAT_H
#define CHAT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"
#include "opcode.h"
#include "sockutil.h"
#include "server.h"

typedef enum opcode
{
    OPCODE_PING  = 0x01, // Respond with PONG
    OPCODE_ECHO  = 0x02, // Return the provided message
    OPCODE_QUIT  = 0x03, // Close client connection
    OPCODE_LOGIN = 0x04, // Login with credentials
} opcode_t;

typedef struct session
{
    char     * p_username;    // Pointer to username
    char     * p_password;    // Pointer to password
    uint8_t    username_size; // Size of username in bytes
    uint8_t    password_size; // Size of password in bytes
    uint64_t   session_id;    // Unique session ID assigned after login
} session_t;

typedef struct request
{
    uint8_t    opcode;    // Opcode
    uint32_t   size;      // Size of payload in network byte order
    char     * p_payload; // Pointer to payload
} request_t;

typedef struct response
{
    uint8_t    status;    // Opcode
    uint32_t   size;      // Size of payload in network byte order
    char     * p_payload; // Pointer to payload
} response_t;

/*!
 * @brief Run client data recv/send loop
 *
 * @param[in] p_server Pointer to server
 * @param[in] p_client Pointer to client
 *
 * @return Status of operation
 */
status_t chat_client_run(server_t * p_server, client_t * p_client);

#endif /* CHAT_H */

/*** end of file ***/
