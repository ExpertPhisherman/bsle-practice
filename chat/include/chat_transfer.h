/** @file chat_transfer.h
 *
 * @brief Transfer opcodes header: request, respond, file_send
 *
 * @par
 *
 */

#ifndef CHAT_TRANSFER_H
#define CHAT_TRANSFER_H

#include "chat_internal.h"
#include "chat_basic.h"
#include "chat_msg.h"

/*!
 * @brief Request PM or file transfer to user
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_request(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Respond to PM or file transfer request from user
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_respond(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Send file to server for relay
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_file_send(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

#endif /* CHAT_TRANSFER_H */

/*** end of file ***/
