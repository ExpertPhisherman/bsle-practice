/** @file opcode.h
 *
 * @brief Opcode header
 *
 * @par
 *
 */

#ifndef OPCODE_H
#define OPCODE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "common.h"
#include "chat.h"
#include "ht.h"

typedef struct session session_t;
typedef struct request request_t;
typedef struct response response_t;

/*!
 * @brief Logic for login
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_login(session_t * p_session,
                      request_t * p_request,
                      response_t * p_response);

/*!
 * @brief Logic for logout
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_logout(session_t * p_session,
                       request_t * p_request,
                       response_t * p_response);

#endif /* OPCODE_H */

/*** end of file ***/
