/** @file chat.h
 *
 * @brief Chat server header
 *
 * @par
 *
 */

#ifndef CHAT_H
#define CHAT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "common.h"
#include "server.h"
#include "chat_internal.h"
#include "server.h"
#include "sockutil.h"
#include "ht.h"
#include "sll.h"

/*!
 * @brief Create chat server
 *
 * @param[in] p_hints Pointer to server hints
 *
 * @return Pointer to chat server
 */
server_t * chat_server_create(server_t * p_hints);

/*!
 * @brief Destroy chat server
 *
 * @param[in] p_server Pointer to chat server
 *
 * @return Status of operation
 */
status_t chat_server_destroy(server_t * p_server);

/*!
 * @brief Single recv/handle/send iteration
 *
 * @param[in] p_server Pointer to server
 * @param[in] p_client Pointer to client
 *
 * @return Status of operation
 */
status_t chat_client_run(server_t * p_server, client_t * p_client);

/*!
 * @brief Initialize per-client state into p_clientdata
 *
 * @param[in] p_server Pointer to server
 * @param[in] p_client Pointer to client
 *
 * @return Status of operation
 */
status_t chat_client_init(server_t * p_server, client_t * p_client);

/*!
 * @brief Free per-client state from p_clientdata
 *
 * @param[in] p_server Pointer to server
 * @param[in] p_client Pointer to client
 *
 * @return Status of operation
 */
status_t chat_client_free(server_t * p_server, client_t * p_client);

#endif /* CHAT_H */

/*** end of file ***/
