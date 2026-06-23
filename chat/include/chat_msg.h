/** @file chat_msg.h
 *
 * @brief Message opcodes header: msg_send, msg_recv
 *
 * @par
 *
 */

#ifndef CHAT_MSG_H
#define CHAT_MSG_H

#include "chat_internal.h"
#include "chat_basic.h"

/*!
 * @brief Send MSG_RECV packet to a single session
 *
 * @param[in] p_session  Pointer to session
 * @param[in] flag       Message flag
 * @param[in] p_msg      Pointer to message
 * @param[in] msg_size   Size of message in bytes
 *
 * @return Status of operation
 */
status_t msg_send(
    session_t     * p_session,
    uint8_t         flag,
    uint8_t const * p_msg,
    uint16_t        msg_size
);

/*!
 * @brief Send MSG_RECV to every session in room matching username
 *
 * @param[in] p_room         Pointer to room
 * @param[in] p_username     Pointer to username
 * @param[in] username_size  Size of username in bytes
 * @param[in] flag           Message flag
 * @param[in] p_msg          Pointer to message
 * @param[in] msg_size       Size of message in bytes
 *
 * @return Status of operation
 */
status_t msg_send_username(
    room_t        * p_room,
    uint8_t       * p_username,
    uint16_t        username_size,
    uint8_t         flag,
    uint8_t const * p_msg,
    uint16_t        msg_size
);

/*!
 * @brief Broadcast MSG_RECV to every logged-in session in room
 *
 * @param[in] p_room    Pointer to room
 * @param[in] flag      Message flag
 * @param[in] p_msg     Pointer to message
 * @param[in] msg_size  Size of message in bytes
 *
 * @return Status of operation
 */
status_t msg_send_room(
    room_t        * p_room,
    uint8_t         flag,
    uint8_t const * p_msg,
    uint16_t        msg_size
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

#endif /* CHAT_MSG_H */

/*** end of file ***/
