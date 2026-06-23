/** @file chat_internal.h
 *
 * @brief Chat server internal types and helpers header
 *
 * @par
 *
 */

#ifndef CHAT_INTERNAL_H
#define CHAT_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "common.h"
#include "server.h"
#include "sockutil.h"
#include "ht.h"
#include "sll.h"

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

typedef struct session
{
    server_t * p_server;        // Pointer to server
    client_t * p_client;        // Pointer to client
    int        sockfd;          // Client socket file descriptor
    uint8_t  * p_username;      // Pointer to username
    uint8_t  * p_password;      // Pointer to password
    uint16_t   username_size;   // Size of username in bytes
    uint16_t   password_size;   // Size of password in bytes
    uint32_t   session_id;      // Unique session ID assigned after login
    room_t   * p_room;          // Pointer to current room
    uint8_t  * p_user_allow;    // Pointer to username that file is allowed from
    uint16_t   user_allow_size; // Size of username that file is allowed from
} session_t;

typedef status_t (*opcode_func_t)(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

typedef enum opcode
{
    OPCODE_DEFAULT    = 0x00, // Default opcode in case of unknown
    OPCODE_PING       = 0x01, // Respond with PONG
    OPCODE_ECHO       = 0x02, // Return the provided message
    OPCODE_QUIT       = 0x03, // Close client connection
    OPCODE_LOGIN      = 0x04, // Log in with credentials
    OPCODE_LOGOUT     = 0x05, // Log out
    OPCODE_MSG_SEND   = 0x06, // Send message to all users in room
    OPCODE_MSG_RECV   = 0x07, // Receive single message
    OPCODE_JOIN       = 0x08, // Join or create room
    OPCODE_LIST       = 0x09, // List available rooms or users in current room
    OPCODE_REQUEST    = 0x0a, // Request PM or file transfer to user
    OPCODE_RESPOND    = 0x0b, // Respond to PM or file transfer request
    OPCODE_FILE_SEND  = 0x0c, // Send file to server for relay
    OPCODE_PROMOTE    = 0x0d, // Promote user to admin
    OPCODE_DISCONNECT = 0x0e, // Disconnect user from server
    OPCODE_DELETE     = 0x0f, // Delete room
} opcode_t;

typedef enum retcode
{
    RETCODE_SUCCESS       = 0x01, // Server action was successful
    RETCODE_SESSION_ERROR = 0x02, // Provided session ID was invalid or expired
    RETCODE_OVERFLOW      = 0x03, // Size exceeds g_max_packet_size
    RETCODE_PENDING       = 0x04, // User already has a pending request
    RETCODE_NOT_PENDING   = 0x05, // User has no pending request to respond to
    RETCODE_DUPLICATE     = 0x06, // User already exists in room
    RETCODE_UNALLOWED     = 0x07, // Receiving user hasn't allowed file transfer
    RETCODE_UNAUTHORIZED  = 0x08, // User isn't authorized to perform action
    RETCODE_FAILURE       = 0xff, // Server action failed
} retcode_t;

typedef struct appdata
{
    uint32_t          next_session_id; // Next session ID to assign
    ht_t            * p_cred_store;    // Pointer to credential storage
    sll_t           * p_room_store;    // Pointer to room storage
    ht_t            * p_admins;        // Pointer to admin hash table
    ht_t            * p_session_store; // Pointer to online session lookup table
    opcode_func_t   * pp_opcode_funcs; // Pointer to opcode function array
    pthread_mutex_t   lock;            // Mutex lock for read/write control
} appdata_t;

typedef struct state
{
    session_t  session;  // Session
    request_t  request;  // Request reused across iterations
    response_t response; // Response reused across iterations
} state_t;

typedef enum field_size
{
    FIELD_SIZE_OPCODE     = 1,
    FIELD_SIZE_FLAG       = 1,
    FIELD_SIZE_PADDING    = 1,
    FIELD_SIZE_RETCODE    = 1,
    FIELD_SIZE_SIZE       = 2,
    FIELD_SIZE_SESSION_ID = 4,
} field_size_t;

typedef enum field_offset
{
    FIELD_OFFSET_OPCODE  = 0,
    FIELD_OFFSET_RETCODE = 1,
} field_offset_t;

typedef enum list_flag
{
    LIST_FLAG_ROOM = 0x00,
    LIST_FLAG_USER = 0x01,
} list_flag_t;

typedef enum req_flag_type
{
    REQ_FLAG_TYPE_PM   = 0x00,
    REQ_FLAG_TYPE_FILE = 0x01,
} req_flag_type_t;

typedef enum resp_flag_type
{
    RESP_FLAG_TYPE_PM   = 0x00,
    RESP_FLAG_TYPE_FILE = 0x01,
} resp_flag_type_t;

typedef enum resp_flag_choice
{
    RESP_FLAG_CHOICE_ACCEPT  = 0x00,
    RESP_FLAG_CHOICE_DECLINE = 0x01,
} resp_flag_choice_t;

typedef enum msg_flag
{
    MSG_FLAG_MSG   = 0x00,
    MSG_FLAG_NOTIF = 0x01,
    MSG_FLAG_LIST  = 0x02,
    MSG_FLAG_JOIN  = 0x03,
    MSG_FLAG_FILE  = 0x04,
} msg_flag_t;

typedef struct __attribute__((packed)) ping_hdr
{
    uint8_t  padding;
    uint32_t session_id;
} ping_hdr_t;

typedef struct __attribute__((packed)) echo_hdr
{
    uint8_t  padding;
    uint16_t payload_size;
    uint32_t session_id;
} echo_hdr_t;

typedef struct __attribute__((packed)) quit_hdr
{
    uint8_t padding;
} quit_hdr_t;

typedef struct __attribute__((packed)) login_hdr
{
    uint8_t  padding;
    uint16_t username_size;
    uint16_t password_size;
} login_hdr_t;

typedef struct __attribute__((packed)) logout_hdr
{
    uint8_t  padding;
    uint32_t session_id;
} logout_hdr_t;

typedef struct __attribute__((packed)) msg_send_hdr
{
    uint8_t  padding;
    uint16_t msg_size;
    uint32_t session_id;
} msg_send_hdr_t;

typedef struct __attribute__((packed)) join_hdr
{
    uint8_t  b_private;
    uint16_t room_name_size;
    uint32_t session_id;
} join_hdr_t;

typedef struct __attribute__((packed)) list_hdr
{
    uint8_t  flag;
    uint32_t session_id;
} list_hdr_t;

typedef struct __attribute__((packed)) req_hdr
{
    uint8_t  flag_type;
    uint16_t username_size;
    uint32_t session_id;
} req_hdr_t;

typedef struct __attribute__((packed)) resp_hdr
{
    uint8_t  flag_type;
    uint8_t  flag_choice;
    uint16_t username_size;
    uint32_t session_id;
} resp_hdr_t;

typedef struct __attribute__((packed)) file_send_hdr
{
    uint8_t  padding;
    uint16_t username_size;
    uint16_t filename_size;
    uint32_t file_size;
    uint32_t session_id;
} file_send_hdr_t;

typedef struct __attribute__((packed)) file_recv_hdr
{
    uint16_t filename_size;
    uint32_t file_size;
} file_recv_hdr_t;

typedef struct __attribute__((packed)) promote_hdr
{
    uint8_t  padding;
    uint16_t username_size;
    uint32_t session_id;
} promote_hdr_t;

typedef struct __attribute__((packed)) disconnect_hdr
{
    uint8_t  padding;
    uint16_t username_size;
    uint32_t session_id;
} disconnect_hdr_t;

typedef struct __attribute__((packed)) delete_hdr
{
    uint8_t  padding;
    uint16_t room_name_size;
    uint32_t session_id;
} delete_hdr_t;

typedef struct __attribute__((packed)) msg_recv_hdr
{
    uint8_t  opcode;
    uint8_t  retcode;
    uint8_t  flag;
    uint16_t msg_size;
} msg_recv_hdr_t;


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
 * @brief Get session by a username in room
 *
 * @param[in] p_room        Pointer to room
 * @param[in] p_username    Pointer to username
 * @param[in] username_size Size of username in bytes
 *
 * @return Boolean if username is in room
 */
session_t * session_by_username(
    room_t   * p_room,
    uint8_t  * p_username,
    uint16_t   username_size
);

/*!
 * @brief Check if credentials length and content are valid
 *
 * @param[in] p_session Pointer to session
 *
 * @return Boolean if credentials are valid
 */
bool user_creds_valid(session_t * p_session);

#endif /* CHAT_INTERNAL_H */

/*** end of file ***/
