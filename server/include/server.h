/** @file server.h
 *
 * @brief Generic TCP server
 *
 * @par
 *
 */

#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include "tpool.h"
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct server server_t;
typedef struct client client_t;
typedef struct session session_t;
typedef struct registry registry_t;

typedef struct server
{
    uint16_t      lport;      // Local port
    int           backlog;    // Number of connection requests to queue
    bool          b_verbose;  // Verbosity
    int           sockfd;     // Socket file descriptor
    tpool_t     * p_tm;       // Pointer to thread pool
    registry_t  * p_registry; // Pointer to client registry
    status_t   (*handle_session)(session_t * p_session);
} server_t;

typedef struct client
{
    uint16_t rport;  // Remote port
    int      sockfd; // Socket file descriptor
} client_t;

typedef struct session
{
    server_t * p_server; // Pointer to server
    client_t * p_client; // Pointer to client
} session_t;

typedef struct registry
{
    client_t        ** pp_clients; // Double pointer to clients
    size_t             count;      // Current population of array
    pthread_mutex_t    lock;       // Mutex lock for read/write control
} registry_t;

/*!
 * @brief Create server
 *
 * @param[in] p_hints Pointer to server hints
 *
 * @return Pointer to server
 */
server_t * server_create(server_t * p_hints);

/*!
 * @brief Run server accept loop
 *
 * @param[in] p_server Pointer to server
 *
 * @return Status of operation
 */
status_t server_run(server_t * p_server);

/*!
 * @brief Destroy server
 *
 * @param[in] p_server Pointer to server
 *
 * @return Status of operation
 */
status_t server_destroy(server_t * p_server);

#endif /* SERVER_H */

/*** end of file ***/
