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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "common.h"
#include "server.h"
#include "chat_opcode.h"
#include "sockutil.h"
#include "safe.h"
#include "ht.h"
#include "sll.h"

typedef struct session  session_t;
typedef struct request  request_t;
typedef struct response response_t;

typedef status_t (*opcode_func_t)(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

typedef enum opcode
{
    OPCODE_DEFAULT = 0x00, // Default opcode in case of unknown
    OPCODE_PING    = 0x01, // Respond with PONG
    OPCODE_ECHO    = 0x02, // Return the provided message
    OPCODE_QUIT    = 0x03, // Close client connection
    OPCODE_LOGIN   = 0x04, // Log in with credentials
    OPCODE_LOGOUT  = 0x05, // Log out
} opcode_t;

typedef struct session
{
    server_t * p_server;      // Pointer to server
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
    uint8_t    status;    // Status 0x00 for success, 0x01 for failure
    uint32_t   size;      // Size of payload in network byte order
    char     * p_payload; // Pointer to payload
} response_t;

typedef struct appdata
{
    safe_ht_t     * p_cred_store;    // Pointer to credential store
    opcode_func_t * pp_opcode_funcs; // Double pointer to opcode function array
} appdata_t;

/*!
 * @brief Create chat server
 *
 * @param[in] p_hints  Pointer to server hints
 * @param[in] capacity Current number of buckets
 *
 * @return Pointer to chat server
 */
server_t * chat_server_create(server_t * p_hints, size_t capacity);

/*!
 * @brief Destroy chat server
 *
 * @param[in] p_server Pointer to chat server
 *
 * @return Status of operation
 */
status_t chat_server_destroy(server_t * p_server);

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
