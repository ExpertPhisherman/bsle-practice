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
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "common.h"
#include "chat.h"
#include "ht.h"

typedef struct session session_t;
typedef struct request request_t;
typedef struct response response_t;

/*!
 * @brief djb2 hash function
 *
 * @param[in] p_response Pointer to response
 * @param[in] p_fmt      Pointer to format string to populate
 *
 * @return 64-bit hash digest
 */
status_t write_response(
    response_t * p_response,
    char const * p_fmt,
    ...
);

/*!
 * @brief Default opcode in case of unknown
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_default(
    session_t * p_session,
    request_t * p_request,
    response_t * p_response
);

/*!
 * @brief Respond with PONG
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_ping(
    session_t * p_session,
    request_t * p_request,
    response_t * p_response
);

/*!
 * @brief Return the provided message
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_echo(
    session_t * p_session,
    request_t * p_request,
    response_t * p_response
);

/*!
 * @brief Close client connection
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_quit(
    session_t * p_session,
    request_t * p_request,
    response_t * p_response
);

/*!
 * @brief Log in with credentials
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_login(
    session_t * p_session,
    request_t * p_request,
    response_t * p_response
);

/*!
 * @brief Log out
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
status_t opcode_logout(
    session_t * p_session,
    request_t * p_request,
    response_t * p_response
);

#endif /* OPCODE_H */

/*** end of file ***/
