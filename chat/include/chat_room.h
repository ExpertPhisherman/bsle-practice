/** @file chat_room.h
 *
 * @brief Room opcodes header: join, list
 *
 * @par
 *
 */

#ifndef CHAT_ROOM_H
#define CHAT_ROOM_H

#include "chat_internal.h"
#include "chat_basic.h"
#include "chat_msg.h"

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

#endif /* CHAT_ROOM_H */

/*** end of file ***/
