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
typedef struct room              room_t;
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
    OPCODE_MSG_SEND = 0x06, // Send message to all users in room
    OPCODE_MSG_RECV = 0x07, // Receive single message
    OPCODE_JOIN     = 0x08, // Join or create room
    OPCODE_LIST     = 0x09, // List all available rooms or users in current room
    OPCODE_REQUEST  = 0x0a, // Request PM or file transfer to user
    OPCODE_RESPOND  = 0x0b, // Respond to PM or file transfer request from user
} opcode_t;

typedef enum retcode
{
    RETCODE_SUCCESS       = 0x01, // Server action was successful
    RETCODE_SESSION_ERROR = 0x02, // Provided session ID was invalid or expired
    RETCODE_OVERFLOW      = 0x03, // Size exceeds g_max_packet_size
    RETCODE_PENDING       = 0x04, // User already has a pending request
    RETCODE_NOT_PENDING   = 0x05, // User has no pending request to respond to
    RETCODE_DUPLICATE     = 0x06, // User already exists in room
    RETCODE_FAILURE       = 0xff, // Server action failed
} retcode_t;

typedef struct session
{
    server_t * p_server;      // Pointer to server
    client_t * p_client;      // Pointer to client
    int        sockfd;        // Client socket file descriptor
    uint8_t  * p_username;    // Pointer to username
    uint8_t  * p_password;    // Pointer to password
    uint16_t   username_size; // Size of username in bytes
    uint16_t   password_size; // Size of password in bytes
    uint32_t   session_id;    // Unique session ID assigned after login
    room_t   * p_room;        // Pointer to current room
} session_t;

typedef struct request
{
    uint8_t    opcode;     // Operation code
    uint32_t   session_id; // Session ID network byte order, unauthenticated = 0
    uint8_t  * p_packet;   // Pointer to packet
    uint32_t   size;       // Size of request packet in bytes
} request_t;

typedef struct response
{
    uint8_t   opcode;   // Operation code
    uint8_t   retcode;  // Return code
    uint8_t * p_packet; // Pointer to packet
    uint32_t  size;     // Size of response packet in bytes
} response_t;

typedef struct room
{
    uint8_t  * p_name;      // Pointer to name
    uint16_t   name_size;   // Size of name in bytes
    sll_t    * p_sessions;  // Pointer to list of members' sessions
    ht_t     * p_pm_reqs;   // Pointer to PM requests
    ht_t     * p_file_reqs; // Pointer to file transfer requests
    bool       b_private;   // Boolean if room is private (direct message)
    uint8_t  * p_user1;     // Name of first allowed private user
    uint16_t   user1_size;  // Size of first allowed private user in bytes
    uint8_t  * p_user2;     // Name of second allowed private user
    uint16_t   user2_size;  // Size of second allowed private user in bytes
} room_t;

typedef struct appdata
{
    uint32_t          next_session_id; // Next session ID to assign
    ht_t            * p_cred_store;    // Pointer to credential storage
    sll_t           * p_room_store;    // Pointer to room storage
    opcode_func_t   * pp_opcode_funcs; // Pointer to opcode function array
    pthread_mutex_t   lock;            // Mutex lock for read/write control
} appdata_t;

typedef struct state
{
    session_t  session;  // Session
    request_t  request;  // Request reused across iterations
    response_t response; // Response reused across iterations
} state_t;

/*!
 * @brief Create chat server
 *
 * @param[in] p_hints Pointer to server hints
 *
 * @return Pointer to chat server
 */
server_t * chat_server_create(server_t * p_hints);

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

/*!
 * @brief Create room
 *
 * @param[in] p_name    Pointer to name
 * @param[in] name_size Size of name in bytes
 *
 * @return Pointer to room
 */
room_t * room_create(char const * p_name, uint16_t name_size);

/*!
 * @brief Destroy room
 *
 * @param[in] p_data Pointer to room
 *
 * @return void
 */
void room_destroy(void * p_data);

/*!
 * @brief Affect session with login
 *
 * @param[in] p_session Pointer to session
 * @param[in] p_appdata Pointer to application data
 *
 * @return Session ID
 */
uint32_t user_login(session_t * p_session, appdata_t * p_appdata);

/*!
 * @brief Affect session with logout
 *
 * @param[in] p_session Pointer to session
 * @param[in] p_appdata Pointer to application data
 *
 * @return Status of operation
 */
status_t user_logout(session_t * p_session, appdata_t * p_appdata);

/*!
 * @brief Affect session with join room
 *
 * @param[in] p_session Pointer to session
 * @param[in] p_appdata Pointer to application data
 *
 * @return Status of operation
 */
status_t user_join(session_t * p_session, appdata_t * p_appdata);

/*!
 * @brief Affect session with leave room
 *
 * @param[in] p_session Pointer to session
 * @param[in] p_appdata Pointer to application data
 *
 * @return Status of operation
 */
status_t user_leave(session_t * p_session, appdata_t * p_appdata);

/*!
 * @brief Check if username is in room
 *
 * @param[in] p_room        Pointer to room
 * @param[in] p_username    Pointer to username
 * @param[in] username_size Size of username in bytes
 *
 * @return Boolean if username is in room
 */
bool username_in_room(
    room_t   * p_room,
    uint8_t  * p_username,
    uint16_t   username_size
);

/*!
 * @brief Check if credentials length is valid
 *
 * @param[in] p_session Pointer to session
 * @param[in] p_appdata Pointer to application data
 *
 * @return Boolean if credentials length is valid
 */
bool user_creds_len_valid(session_t * p_session, appdata_t * p_appdata);

/*!
 * @brief Check if credentials content is valid
 *
 * @param[in] p_session Pointer to session
 * @param[in] p_appdata Pointer to application data
 *
 * @return Boolean if credentials content is valid
 */
bool user_creds_content_valid(session_t * p_session, appdata_t * p_appdata);

#endif /* CHAT_H */

/*** end of file ***/
