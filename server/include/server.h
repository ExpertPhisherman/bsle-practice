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

typedef struct registry registry_t;

typedef struct server
{
    uint16_t     lport;      // Local port
    int          sockfd;     // Socket file descriptor
    int          backlog;    // Number of connection requests to queue
    bool         b_verbose;  // Verbosity
    tpool_t    * p_tm;       // Pointer to thread pool
    registry_t * p_registry; // Pointer to client registry
} server_t;

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

// Client session
typedef struct session
{
    uint16_t     lport;      // Local port
    uint16_t     rport;      // Remote port
    int          sockfd;     // Socket file descriptor
    int          backlog;    // Number of connection requests to queue
    bool         b_verbose;  // Verbosity
    tpool_t    * p_tm;       // Pointer to thread pool
    registry_t * p_registry; // Pointer to client registry
} session_t;

// Client registry
typedef struct registry
{
    session_t       ** pp_sessions; // Double pointer to sessions
    size_t             count;       // Current population of array
    pthread_mutex_t    lock;        // Mutex lock for read/write control
} registry_t;

/*!
 * @brief Create registry
 *
 * @param[in] void
 *
 * @return Pointer to registry
 */
registry_t * registry_create(void);

/*!
 * @brief Destroy registry
 *
 * @param[in] p_registry Pointer to registry
 *
 * @return Status of operation
 */
status_t registry_destroy(registry_t * p_registry);

/*!
 * @brief Add session to registry
 *
 * @param[in] p_registry Pointer to registry
 * @param[in] p_session  Pointer to session to add
 *
 * @return Status of operation
 */
status_t registry_add(registry_t * p_registry, session_t * p_session);

/*!
 * @brief Remove file descriptor from registry
 *
 * @param[in] p_registry Pointer to registry
 * @param[in] p_session  Pointer to session to remove
 *
 * @return Status of operation
 */
status_t registry_remove(registry_t * p_registry, session_t * p_session);

/*!
 * @brief Wrapper for handle_client
 *
 * @param[in] p_arg Pointer to argument
 *
 * @return void
 */
void handle_client_wrapper(void * p_arg);

/*!
 * @brief Create server session
 *
 * @param[in] p_hints Pointer to session hints
 *
 * @return Pointer to server session
 */
session_t * server_session_create(session_t * p_hints);

/*!
 * @brief Destroy server session
 *
 * @param[in] p_server_session Pointer to server session
 *
 * @return Status of operation
 */
status_t server_session_destroy(session_t * p_server_session);

/*!
 * @brief Create client session
 *
 * @param[in] p_server_session Pointer to server session
 *
 * @return Pointer to client session
 */
session_t * client_session_create(session_t * p_server_session);

/*!
 * @brief Destroy client session
 *
 * @param[in] p_client_session Pointer to client session
 *
 * @return Status of operation
 */
status_t client_session_destroy(session_t * p_client_session);

/*!
 * @brief Handle client session
 *
 * @param[in] p_session Pointer to session
 *
 * @return Status of operation
 */
status_t handle_client(session_t * p_session);

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

#endif /* SERVER_H */

/*** end of file ***/
