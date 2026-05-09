/** @file server.h
 *
 * @brief Generic TCP server header
 *
 * @par
 *
 */

#ifndef SERVER_H
#define SERVER_H

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
#include "tpool.h"

typedef struct server             server_t;
typedef struct client             client_t;
typedef struct server_client_pair server_client_pair_t;
typedef struct registry           registry_t;

typedef status_t (*client_func_t)(server_t * p_server, client_t * p_client);

typedef struct server
{
    uint16_t        lport;         // Local port
    char          * p_lhost;       // Pointer to local host IP address
    int             backlog;       // Number of connection requests to queue
    bool            b_verbose;     // Verbosity
    int             sockfd;        // Server socket file descriptor
    int             epollfd;       // Epoll file descriptor
    tpool_t       * p_tm;          // Pointer to thread pool
    registry_t    * p_registry;    // Pointer to client registry
    client_func_t   p_client_run;  // Pointer to single iteration handler
    client_func_t   p_client_init; // Pointer to per-client init callback
    client_func_t   p_client_free; // Pointer to per-client free callback
    void          * p_appdata;     // Pointer to application data
} server_t;

typedef struct client
{
    uint16_t   rport;        // Remote port
    char     * p_rhost;      // Pointer to remote host IP address
    int        sockfd;       // Socket file descriptor
    void     * p_clientdata; // Pointer to per-client application state
} client_t;

typedef struct server_client_pair
{
    server_t * p_server; // Pointer to server
    client_t * p_client; // Pointer to client
} server_client_pair_t;

typedef struct registry
{
    client_t        ** pp_clients; // Double pointer to clients
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
 * @brief Run server epoll loop
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
