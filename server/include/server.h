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
extern volatile sig_atomic_t g_keep_running;

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

// Client registry
typedef struct registry
{
    int             * sockfds; // Pointer to array of file descriptors
    size_t            count;   // Current population of array
    pthread_mutex_t   lock;    // Mutex lock for read/write control
} registry_t;

// Session
typedef struct session
{
    uint16_t     lport;      // Local port
    uint16_t     rport;      // Remote port
    int          server_fd;  // Server listening socket file descriptor
    int          client_fd;  // Client connected socket file descriptor
    int          backlog;    // Number of connection requests to queue
    bool         b_verbose;  // Verbosity
    tpool_t    * p_tm;       // Pointer to thread pool
    registry_t * p_registry; // Pointer to client registry
} session_t;

/*!
 * @brief Prepare, bind, and listen on socket
 *
 * @param[in] p_session Pointer to session
 *
 * @return Status of operation
 */
status_t server_sock(session_t * p_session);

/*!
 * @brief Accept and establish connection on socket
 *
 * @param[in] p_session Pointer to session
 *
 * @return Status of operation
 */
status_t client_socket(session_t * p_session);

/*!
 * @brief Handle client session
 *
 * @param[in] p_session Pointer to session
 *
 * @return Status of operation
 */
status_t handle_client(session_t * p_session);

/*!
 * @brief Send all data to client
 *
 * @param[in] sockfd Socket file descriptor to send data to
 * @param[in] p_buf  Buffer to send
 * @param[in] size   Number of bytes to send
 *
 * @return Status of operation
 */
status_t sendall(int sockfd, void * p_buf, size_t size);

/*!
 * @brief Receive all data from client
 *
 * @param[in] sockfd Socket file descriptor to receive data from
 * @param[in] p_buf  Buffer to receive into
 * @param[in] size   Number of bytes to receive
 *
 * @return Status of operation
 */
status_t recvall(int sockfd, void * p_buf, size_t size);

#endif /* SERVER_H */

/*** end of file ***/
