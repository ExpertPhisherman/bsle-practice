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
#include "chat_basic.h"
#include "chat_auth.h"
#include "chat_msg.h"
#include "chat_room.h"
#include "chat_transfer.h"
#include "chat_admin.h"
#include "sockutil.h"
#include "ht.h"
#include "sll.h"

#define DEFAULT_LPORT 3333

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
