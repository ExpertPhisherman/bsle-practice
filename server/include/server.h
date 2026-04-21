/** @file server.h
 *
 * @brief Echo header
 *
 * @par
 * Basic TCP server
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

extern uint32_t const drain_chunk_size;
extern uint16_t const max_port;
extern int const default_backlog;
extern _Atomic sig_atomic_t g_keep_running;

// App specific variables
extern uint16_t const default_lport;
extern uint32_t const max_payload_size;
extern uint32_t const max_clients;
extern uint32_t const worker_threads;

typedef enum opcode
{
    OPCODE_PING = 0x01, // Respond with PONG
    OPCODE_ECHO = 0x02, // Return the provided message
    OPCODE_QUIT = 0x03, // Close client connection
} opcode_t;

typedef struct request
{
    uint8_t    opcode;    // Opcode
    uint32_t   size;      // Size of payload in network byte order
    char     * p_payload; // Pointer to payload
} request_t;

typedef struct response
{
    uint8_t    status;    // Opcode
    uint32_t   size;      // Size of payload in network byte order
    char     * p_payload; // Pointer to payload
} response_t;

typedef struct client
{
    uint16_t     rport;     // Remote port
    int          sockfd;    // Socket file descriptor
} client_t;

// Client registry
typedef struct registry
{
    client_t        ** pp_clients; // Double pointer to clients
    size_t             count;      // Current population of array
    pthread_mutex_t    lock;       // Mutex lock for read/write control
} registry_t;

typedef struct server
{
    uint16_t     lport;      // Local port
    int          sockfd;     // Socket file descriptor
    int          backlog;    // Number of connection requests to queue
    bool         b_verbose;  // Verbosity
    tpool_t    * p_tm;       // Pointer to thread pool
    registry_t * p_registry; // Pointer to client registry
} server_t;

typedef struct session
{
    server_t * p_server; // Pointer to server
    client_t * p_client; // Pointer to client
} session_t;

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
 * @brief Destroy server
 *
 * @param[in] p_server Pointer to server
 *
 * @return Status of operation
 */
status_t server_destroy(server_t * p_server);

/*!
 * @brief Create client
 *
 * @param[in] p_server Pointer to server
 *
 * @return Pointer to client
 */
client_t * client_create(server_t * p_server);

/*!
 * @brief Destroy client
 *
 * @param[in] p_server Pointer to server
 * @param[in] p_client Pointer to client
 *
 * @return Status of operation
 */
status_t client_destroy(server_t * p_server, client_t * p_client);

/*!
 * @brief Handle session
 *
 * @param[in] p_session Pointer to session
 *
 * @return Status of operation
 */
status_t handle_session(session_t * p_session);

#endif /* SERVER_H */

/*** end of file ***/
