/** @file chat_auth.h
 *
 * @brief Auth opcodes header: login, logout
 *
 * @par
 *
 */

#ifndef CHAT_AUTH_H
#define CHAT_AUTH_H

#include "chat_internal.h"
#include "chat_basic.h"

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

#endif /* CHAT_AUTH_H */

/*** end of file ***/
