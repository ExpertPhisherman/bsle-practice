/** @file chat.h
 *
 * @brief Chat server header
 *
 * @par
 *
 */

#ifndef CHAT_H
#define CHAT_H

#include "common.h"
#include "server.h"

#define DEFAULT_LPORT 3333

typedef struct server server_t;

/*!
 * @brief Initialize chat server
 *
 * @param[in] p_server Pointer to server
 *
 * @return Status of operation
 */
status_t chat_server_init(server_t * p_server);

/*!
 * @brief Free chat server
 *
 * @param[in] p_server Pointer to server
 *
 * @return Status of operation
 */
status_t chat_server_free(server_t * p_server);

#endif /* CHAT_H */

/*** end of file ***/
