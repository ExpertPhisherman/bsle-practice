/** @file server.c
 *
 * @brief Echo server
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

#include "sll.h"
#include "common.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
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

typedef struct session
{
    uint16_t server_port;
    uint16_t client_port;
    int server_sockfd;    // Server listening socket file descriptor
    int client_sockfd;    // Client connected socket file descriptor
    int backlog;
    bool b_verbose;
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
status_t sendall(int sockfd, void const * const p_buf, size_t const size);

/*!
 * @brief Receive all data from client
 *
 * @param[in] sockfd Socket file descriptor to receive data from
 * @param[in] p_buf  Buffer to receive into
 * @param[in] size   Number of bytes to receive
 *
 * @return Status of operation
 */
status_t recvall(int sockfd, void * const p_buf, size_t const size);

#endif /* SERVER_H */

/*** end of file ***/
