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
#include <netinet/in.h>
#include <pthread.h>
#include "common.h"
#include "server.h"
#include "opcode.h"
#include "sockutil.h"
#include "ht.h"

typedef enum opcode
{
    OPCODE_PING  = 0x01, // Respond with PONG
    OPCODE_ECHO  = 0x02, // Return the provided message
    OPCODE_QUIT  = 0x03, // Close client connection
    OPCODE_LOGIN = 0x04, // Login with credentials
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

typedef struct safe_ht
{
    ht_t            * p_ht; // Pointer to hash table
    pthread_mutex_t   lock; // Mutex lock for read/write control
} safe_ht_t;

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
