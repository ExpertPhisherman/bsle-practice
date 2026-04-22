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

extern uint16_t const max_port;
extern _Atomic sig_atomic_t g_keep_running;

// App specific variables
extern uint16_t const default_lport;
extern uint32_t const max_payload_size;
extern uint32_t const max_clients;
extern uint32_t const worker_threads;
extern uint32_t const drain_chunk_size;
extern int const default_backlog;
extern status_t load_app(server_t * p_server);

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
    uint16_t     rport;     // Remote port
    int          sockfd;    // Socket file descriptor
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
 * @brief Wrapper for handle_session
 *
 * @param[in] p_arg Pointer to argument
 *
 * @return void
 */
void handle_session_wrapper(void * p_arg);

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

/*!
 * @brief Send all data to socket
 *
 * @param[in] sockfd Socket to send data to
 * @param[in] p_buf  Buffer to send
 * @param[in] size   Number of bytes to send
 *
 * @return Status of operation
 */
status_t sendall(int sockfd, void * p_buf, size_t size);

/*!
 * @brief Receive all data from socket
 *
 * @param[in] sockfd Socket to receive data from
 * @param[in] p_buf  Buffer to receive into
 * @param[in] size   Number of bytes to receive
 *
 * @return Status of operation
 */
status_t recvall(int sockfd, void * p_buf, size_t size);

/*!
 * @brief Drain bytes from socket
 *
 * @param[in] sockfd Socket file descriptor
 * @param[in] size   Number of bytes to drain
 *
 * @return Status of operation
 */
status_t drain(int sockfd, uint32_t size);

#endif /* SERVER_H */

/*** end of file ***/
