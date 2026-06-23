/** @file chat_internal.h
 *
 * @brief Chat server internal helpers (not part of the public API)
 *
 * @par
 *
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */

#ifndef CHAT_INTERNAL_H
#define CHAT_INTERNAL_H

#include "chat_opcode.h"

/*!
 * @brief Check standard opcode handler arguments for NULL
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
bool opcode_args_valid(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Validate client session ID against server record
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t validate_session(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Send MSG_RECV packet to a single session
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
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
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
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
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
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
 * @brief True if p_session is an admin session
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
bool is_admin(
    session_t * p_session,
    appdata_t * p_appdata
);

#endif /* CHAT_INTERNAL_H */

/*** end of file ***/
