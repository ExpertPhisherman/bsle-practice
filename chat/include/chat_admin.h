/** @file chat_admin.h
 *
 * @brief Admin opcodes header: promote, disconnect, delete
 *
 * @par
 *
 */

#ifndef CHAT_ADMIN_H
#define CHAT_ADMIN_H

#include "chat_internal.h"
#include "chat_basic.h"
#include "chat_msg.h"

/*!
 * @brief True if p_session belongs to an admin user
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_appdata  Pointer to application data
 *
 * @return True if session is an admin
 */
bool is_admin(
    session_t * p_session,
    appdata_t * p_appdata
);

/*!
 * @brief Promote user to admin
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_promote(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Disconnect user from server
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_disconnect(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Delete room
 *
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_delete(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

#endif /* CHAT_ADMIN_H */

/*** end of file ***/
