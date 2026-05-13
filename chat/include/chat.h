/** @file chat.h
 *
 * @brief Chat server header
 *
 * @par
 *
 */

#ifndef CHAT_H
#define CHAT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "common.h"
#include "server.h"
#include "chat_opcode.h"
#include "sockutil.h"
#include "ht.h"
#include "sll.h"

typedef struct session           session_t;
typedef struct request           request_t;
typedef struct response          response_t;
typedef struct appdata           appdata_t;
typedef struct chat_client_state chat_client_state_t;

typedef status_t (*opcode_func_t)(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

typedef enum opcode
{
    OPCODE_DEFAULT  = 0x00, // Default opcode in case of unknown
    OPCODE_PING     = 0x01, // Respond with PONG
    OPCODE_ECHO     = 0x02, // Return the provided message
    OPCODE_QUIT     = 0x03, // Close client connection
    OPCODE_LOGIN    = 0x04, // Log in with credentials
    OPCODE_LOGOUT   = 0x05, // Log out
    OPCODE_MSG_SEND = 0x06, // Receive single message
    OPCODE_MSG_RECV = 0x07, // Receive last seen timestamp, send newer messages
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
    uint8_t    opcode;     // Opcode
    uint32_t   session_id; // Session ID network byte order, unauthenticated = 0
    uint32_t   size;       // Size of payload in network byte order
    char     * p_payload;  // Pointer to payload
} request_t;

typedef struct response
{
    uint8_t    status;    // Status 0x00 for success, 0x01 for failure
    uint32_t   size;      // Size of payload in network byte order
    char     * p_payload; // Pointer to payload
} response_t;

typedef struct appdata
{
    ht_t            * p_cred_store;    // Pointer to credential store
    uint32_t          next_session_id; // Next session ID to assign
    opcode_func_t   * pp_opcode_funcs; // Pointer to opcode function array
    pthread_mutex_t   lock;            // Mutex lock for read/write control
} appdata_t;

typedef struct chat_client_state
{
    session_t  session;  // Session
    request_t  request;  // Request reused across iterations
    response_t response; // Respose reused across iterations
} chat_client_state_t;

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
 * @brief Single recv/handle/send iteration
 *
 * @param[in] p_server Pointer to server
 * @param[in] p_client Pointer to client
 *
 * @return Status of operation
 */
status_t chat_client_run(server_t * p_server, client_t * p_client);

/*!
 * @brief Initialize per-client state into p_clientdata
 *
 * @param[in] p_server Pointer to server
 * @param[in] p_client Pointer to client
 *
 * @return Status of operation
 */
status_t chat_client_init(server_t * p_server, client_t * p_client);

/*!
 * @brief Free per-client state from p_clientdata
 *
 * @param[in] p_server Pointer to server
 * @param[in] p_client Pointer to client
 *
 * @return Status of operation
 */
status_t chat_client_free(server_t * p_server, client_t * p_client);

#endif /* CHAT_H */

/*** end of file ***/
