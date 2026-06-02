/** @file chat_opcode.h
 *
 * @brief Chat opcode header
 *
 * @par
 *
 */

#ifndef CHAT_OPCODE_H
#define CHAT_OPCODE_H

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
#include "sockutil.h"
#include "chat.h"
#include "ht.h"
#include "sll.h"

typedef struct session  session_t;
typedef struct request  request_t;
typedef struct response response_t;

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
    RESP_FLAG_TYPE_PM      = 0x00,
    RESP_FLAG_TYPE_FILE    = 0x01,
} resp_flag_type_t;

typedef enum resp_flag_choice
{
    RESP_FLAG_CHOICE_ACCEPT  = 0x00,
    RESP_FLAG_CHOICE_DECLINE = 0x01,
} resp_flag_choice_t;

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
    uint8_t  padding;
    uint8_t  flag_type;
    uint8_t  flag_choice;
    uint16_t username_size;
    uint32_t session_id;
} resp_hdr_t;

typedef struct __attribute__((packed)) msg_recv_hdr_out
{
    uint8_t  opcode;
    uint8_t  retcode;
    uint16_t msg_size;
} msg_recv_hdr_out_t;

/*!
 * @brief Default opcode in case of unknown
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_default(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Respond with PONG
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_ping(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Return the provided message
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_echo(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Close client connection
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_quit(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Log in with credentials
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_login(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Log out
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_logout(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Send message to all users in room
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_msg_send(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Join room or create it if it doesn't exist
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_join(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief List all available rooms or users in current room
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_list(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

#endif /* CHAT_OPCODE_H */

/*** end of file ***/
