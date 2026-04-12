/** @file server.h
 *
 * @brief Echo header
 *
 * @par
 * Basic TCP server
 */

#ifndef SERVER_H
#define SERVER_H

#define MAX_PORT 65535
#define DRAIN_CHUNK_SIZE 512
#define DEFAULT_BACKLOG 10
#define MAX_PAYLOAD_SIZE 4096
#define WORKER_THREADS 8
#define MAX_CLIENTS 10

#include "sll.h"
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

extern volatile sig_atomic_t g_keep_running;

typedef enum opcode
{
    OPCODE_PING = 0x01, // Respond with PONG
    OPCODE_ECHO = 0x02, // Return the provided message
    OPCODE_QUIT = 0x03  // Close client connection
} opcode_t;

typedef struct request
{
    uint8_t opcode;
    uint32_t size;    // Network byte order
    char * p_payload;
} request_t;

typedef struct response
{
    uint8_t status;
    uint32_t size;    // Network byte order
    char * p_payload;
} response_t;

// Client registry
typedef struct registry
{
    int             sockfds[MAX_CLIENTS]; // Array of registered socket file descriptors
    size_t          count;                // Current population of array
    pthread_mutex_t lock;                 // Mutex lock for read/write control
} registry_t;

// Session
typedef struct session
{
    uint16_t server_port;    // Server port
    uint16_t client_port;    // Client port
    int server_sockfd;       // Server listening socket file descriptor
    int client_sockfd;       // Client connected socket file descriptor
    int backlog;             // Number of connection requests to queue
    bool b_verbose;          // Verbosity
    tpool_t * p_tm;          // Pointer to thread pool
    registry_t * p_registry; // Pointer to client registry
} session_t;

/*!
 * @brief Prepare, bind, and listen on socket
 *
 * @param[in] p_session Pointer to session
 *
 * @return Status of operation
 */
status_t server_socket(session_t * p_session);

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
