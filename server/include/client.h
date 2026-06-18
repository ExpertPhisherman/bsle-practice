/** @file client.h
 *
 * @brief Generic TCP client header
 *
 * @par
 *
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "common.h"
#include "server.h"

typedef struct server server_t;

typedef struct client
{
    uint16_t          rport;        // Remote port
    char            * p_rhost;      // Pointer to remote host IP address
    int               sockfd;       // Socket file descriptor
    int64_t           registry_idx; // Index of client in registry
    void            * p_clientdata; // Pointer to per-client application state
    pthread_mutex_t   lock;         // Mutex lock while sending to client
} client_t;

/*!
 * @brief Dispatch single recv/handle/send iteration to the thread pool,
 *        then re-arm client epoll registration
 *
 * @param[in] p_arg Pointer to server_client_pair_t
 *
 * @return void
 */
void client_run_wrapper(void * p_arg);

/*!
 * @brief Accept new client and initialise per-client state
 *
 * @param[in] p_server Pointer to server
 *
 * @return Pointer to client
 */
client_t * client_create(server_t * p_server);

/*!
 * @brief Destroy client, remove from epoll, shut down socket, call cleanup
 *
 * @param[in] p_server Pointer to server
 * @param[in] p_client Pointer to client
 *
 * @return Status of operation
 */
status_t client_destroy(server_t * p_server, client_t * p_client);

#endif /* CLIENT_H */

/*** end of file ***/
