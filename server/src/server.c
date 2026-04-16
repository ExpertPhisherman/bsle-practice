/** @file server.c
 *
 * @brief Echo server
 *
 * @par
 * Basic TCP server
 */

#include "server.h"

uint32_t const drain_chunk_size = 512u;
uint16_t const max_port = 65535u;
int const default_backlog = 10;
volatile sig_atomic_t g_keep_running = 1;

/*!
 * @brief Drain bytes from socket
 *
 * @param[in] sockfd Socket file descriptor
 * @param[in] size   Number of bytes to drain
 *
 * @return Status of operation
 */
static status_t drain(int sockfd, uint32_t size);

/*!
 * @brief Print request
 *
 * @param[in] p_session Pointer to session
 * @param[in] p_request Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
static status_t print_request(session_t const * p_session, request_t const * p_request);

/*!
 * @brief Print response
 *
 * @param[in] p_session Pointer to session
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
static status_t print_response(session_t const * p_session, response_t const * p_response);

/*!
 * @brief Receive request from client
 *
 * @param[in] sockfd    Socket file descriptor
 * @param[in] p_request Pointer to request
 *
 * @return Status of operation
 */
static status_t recv_request(int sockfd, request_t * p_request, response_t * p_response);

/*!
 * @brief Send response to client
 *
 * @param[in] sockfd     Socket file descriptor
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
static status_t send_response(int sockfd, response_t * p_response);

/*!
 * @brief Add file descriptor to registry
 *
 * @param[in] p_registry Pointer to registry
 * @param[in] sockfd     Socket file descriptor to add
 *
 * @return Boolean if added
 */
static bool registry_add(registry_t * p_registry, int sockfd);

/*!
 * @brief Remove file descriptor from registry
 *
 * @param[in] p_registry Pointer to registry
 * @param[in] sockfd     Socket file descriptor to remove
 *
 * @return Boolean if removed
 */
static bool registry_remove(registry_t * p_registry, int sockfd);

/*!
 * @brief Wrapper for handle_client
 *
 * @param[in] p_arg Pointer to argument
 *
 * @return void
 */
static void handle_client_wrapper(void * p_arg);

status_t
server_sock (session_t * p_session)
{
    status_t status;
    struct sockaddr_in server_addr;

    int yes = 1;

    if (NULL == p_session)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == server_fd)
    {
        perror("socket");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (-1 == setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
    {
        perror("setsockopt");
        if (-1 == close(server_fd))
        {
            perror("close");
        }

        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(p_session->lport);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Catch privileged port as non-root user
    if ((1024u > (p_session->lport)) && (0 != geteuid()))
    {
        fprintf(stderr, "Cannot bind to privileged port %hu [1-1023] as non-root user\n", p_session->lport);
        if (-1 == close(server_fd))
        {
            perror("close");
        }

        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (-1 == bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
    {
        perror("bind");
        if (-1 == close(server_fd))
        {
            perror("close");
        }

        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (-1 == listen(server_fd, p_session->backlog))
    {
        perror("listen");
        if (-1 == close(server_fd))
        {
            perror("close");
        }

        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (p_session->b_verbose)
    {
        char p_ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(server_addr.sin_addr.s_addr), p_ipstr, sizeof(p_ipstr));
        printf("Listening on %s:%hu (server_fd: %d)\n", p_ipstr, p_session->lport, server_fd);
    }

    p_session->server_fd = server_fd;
    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    return status;
}

status_t
client_socket (session_t * p_session)
{
    status_t status;
    int client_fd;

    if (NULL == p_session)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Accept loop
    while (g_keep_running)
    {
        struct sockaddr_in client_addr;

        socklen_t sin_size = sizeof(client_addr);
        client_fd = accept(p_session->server_fd, (struct sockaddr *)&client_addr, &sin_size);
        if (-1 == client_fd)
        {
            if (EINTR == errno)
            {
                continue;
            }

            perror("accept");
            continue;
        }

        uint16_t rport = ntohs(client_addr.sin_port);

        if (p_session->b_verbose)
        {
            char p_ipstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr.s_addr), p_ipstr, sizeof(p_ipstr));
            printf("Accepted connection from %s:%hu (client_fd %d)\n", p_ipstr, rport, client_fd);
        }

        // Multithread client socket connections
        session_t * p_client_session = malloc(sizeof(*p_client_session));
        if (NULL == p_client_session)
        {
            fprintf(stderr, "malloc failed for client_fd %d\n", client_fd);
            if (-1 == close(client_fd))
            {
                perror("close");
            }
            continue;
        }

        *p_client_session = *p_session;
        p_client_session->rport = rport;
        p_client_session->client_fd = client_fd;

        // Register client file descriptor
        if (!registry_add(p_session->p_registry, client_fd))
        {
            fprintf(stderr, "max_clients reached, closing client_fd %d\n", client_fd);
            if (-1 == close(client_fd))
            {
                perror("close");
            }
            free(p_client_session);
            p_client_session = NULL;
            continue;
        }

        if (!tpool_add_work(p_session->p_tm, handle_client_wrapper, p_client_session))
        {
            fprintf(stderr, "tpool_add_work failed\n");
            if (!registry_remove(p_session->p_registry, client_fd))
            {
                fprintf(stderr, "client_fd %d not found in registry\n", client_fd);
            }
            if (-1 == close(client_fd))
            {
                perror("close");
            }
            free(p_client_session);
            p_client_session = NULL;
            continue;
        }
    }

    if (p_session->b_verbose)
    {
        printf("\nShutting down server gracefully...\n");
    }

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    return status;
}

status_t
handle_client (session_t * p_session)
{
    status_t status;

    char * p_recv_buf;
    char * p_send_buf;

    if (NULL == p_session)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_recv_buf = malloc(max_payload_size);
    if (NULL == p_recv_buf)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_send_buf = malloc(max_payload_size);
    if (NULL == p_send_buf)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    };

    while (g_keep_running)
    {
        request_t request = {
            .opcode    = 0u,
            .size      = 0u,
            .p_payload = p_recv_buf
        };

        response_t response = {
            .status    = 0x00,
            .size      = 0u,
            .p_payload = p_send_buf
        };

        memset(p_recv_buf, 0, max_payload_size);
        memset(p_send_buf, 0, max_payload_size);

        status = recv_request(p_session->client_fd, &request, &response);
        if (STATUS_OVERFLOW == status)
        {
            status = print_response(p_session, &response);
            if (STATUS_SUCCESS != status)
            {
                goto cleanup;
            }
            status = send_response(p_session->client_fd, &response);
            if (STATUS_SUCCESS != status)
            {
                goto cleanup;
            }
            continue;
        }
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }

        status = print_request(p_session, &request);
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }

        char const * p_response_payload = "";
        response.status = 0x00;
        switch (request.opcode)
        {
            case OPCODE_PING:
                p_response_payload = "PONG";
                break;

            case OPCODE_ECHO:
                p_response_payload = request.p_payload;
                break;

            case OPCODE_QUIT:
                p_response_payload = "GOODBYE";
                break;

            default:
                response.status = 0x01;
                p_response_payload = "UNKNOWN OPCODE";
                fprintf(stderr, "Unknown opcode from client_fd %d: 0x%02hhx\n", p_session->client_fd, request.opcode);
                break;
        }

        uint32_t host_response_size = strnlen(p_response_payload, max_payload_size);
        response.size = htonl(host_response_size);
        memcpy(response.p_payload, p_response_payload, host_response_size);

        status = print_response(p_session, &response);
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }

        status = send_response(p_session->client_fd, &response);
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }

        if (OPCODE_QUIT == request.opcode)
        {
            // QUIT opcode, close connection
            status = STATUS_SUCCESS;
            goto cleanup;
        }
    }

    status = STATUS_SERVER_DISCONNECT;
    goto cleanup;

cleanup:
    free(p_recv_buf);
    p_recv_buf = NULL;
    free(p_send_buf);
    p_send_buf = NULL;

    return status;
}

status_t
sendall (int sockfd, void * p_buf, size_t size)
{
    status_t status;

    size_t total = 0u;

    while (total < size)
    {
        ssize_t sent = send(sockfd, (uint8_t *)p_buf + total, size - total, 0);
        if (-1 == sent)
        {
            if (EINTR == errno)
            {
                if (!g_keep_running)
                {
                    status = STATUS_SERVER_DISCONNECT;
                    goto cleanup;
                }
                continue;
            }

            status = STATUS_SEND_FAILURE;
            goto cleanup;
        }
        else if (0 == sent)
        {
            status = STATUS_SEND_FAILURE;
            goto cleanup;
        }
        else
        {
            total += (size_t)sent;
        }
    }

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    if (STATUS_SEND_FAILURE == status)
    {
        perror("sendall");
    }

    return status;
}

status_t
recvall (int sockfd, void * p_buf, size_t size)
{
    status_t status;

    size_t total = 0u;

    while (total < size)
    {
        ssize_t recvd = recv(sockfd, (uint8_t *)p_buf + total, size - total, 0);
        if (-1 == recvd)
        {
            switch (errno)
            {
                case EINTR:
                    if (!g_keep_running)
                    {
                        status = STATUS_SERVER_DISCONNECT;
                        goto cleanup;
                    }
                    continue;

                default:
                    status = STATUS_RECV_FAILURE;
                    goto cleanup;
            }
        }
        else if (0 == recvd)
        {
            status = g_keep_running ? STATUS_CLIENT_DISCONNECT : STATUS_SERVER_DISCONNECT;
            goto cleanup;
        }
        else
        {
            total += (size_t)recvd;
        }
    }

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    if (STATUS_RECV_FAILURE == status)
    {
        perror("recvall");
    }
    else if (STATUS_CLIENT_DISCONNECT == status)
    {
        fprintf(stderr, "Abrupt disconnect from client_fd: %d\n", sockfd);
    }
    else
    {
        // Pass
    }

    return status;
}

static status_t
drain (int sockfd, uint32_t size)
{
    status_t status = STATUS_SUCCESS;
    uint8_t * p_buf;

    p_buf = malloc(drain_chunk_size);

    while (0u < size)
    {
        uint32_t chunk = (size < drain_chunk_size) ? size : drain_chunk_size;

        status = recvall(sockfd, p_buf, chunk);
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }

        size -= chunk;
    }

    goto cleanup;

cleanup:
    free(p_buf);
    p_buf = NULL;

    return status;
}

static status_t
print_request (session_t const * p_session, request_t const * p_request)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_session) || (NULL == p_request))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (!p_session->b_verbose)
    {
        goto cleanup;
    }

    uint32_t host_request_size = ntohl(p_request->size);

    printf(
        "========================================\n"
        "Request from client_fd %d:\n"
        "{\n"
        "    opcode : 0x%02hhx\n"
        "    size   : %u\n"
        "    payload: ",
        p_session->client_fd,
        p_request->opcode,
        host_request_size
    );

    display_bytes(p_request->p_payload, host_request_size, " ");
    printf("\n}\n");

cleanup:
    return status;
}

static status_t
print_response (session_t const * p_session, response_t const * p_response)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_session) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (!p_session->b_verbose)
    {
        goto cleanup;
    }

    uint32_t host_response_size = ntohl(p_response->size);

    printf(
        "========================================\n"
        "Response to client_fd %d:\n"
        "{\n"
        "    status : 0x%02hhx\n"
        "    size   : %u\n"
        "    payload: ",
        p_session->client_fd,
        p_response->status,
        host_response_size
    );

    display_bytes(p_response->p_payload, host_response_size, " ");
    printf("\n}\n");

cleanup:
    return status;
}

static status_t
recv_request (int sockfd, request_t * p_request, response_t * p_response)
{
    status_t status;

    if ((NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Receive opcode and payload size in one recvall
    uint8_t p_header_buf[5];
    status = recvall(sockfd, p_header_buf, 5u);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    p_request->opcode = p_header_buf[0];
    memcpy(&(p_request->size), p_header_buf + 1, sizeof(p_request->size));

    uint32_t host_request_size = ntohl(p_request->size);

    // Reject oversized payloads
    if (host_request_size > max_payload_size)
    {
        p_response->status = 0x01;
        p_response->size = htonl((uint32_t)snprintf(
            p_response->p_payload, max_payload_size,
            "Request payload size %u exceeds max_payload_size %u",
            host_request_size, max_payload_size
        ));
        fprintf(stderr, "%s\n", p_response->p_payload);

        status = drain(sockfd, host_request_size);
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }

        status = STATUS_OVERFLOW;
        goto cleanup;
    }

    // Receive payload
    // NOTE: Enforces payload is exactly host_request_size bytes
    status = recvall(sockfd, p_request->p_payload, host_request_size);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

cleanup:
    return status;
}

static status_t
send_response (int sockfd, response_t * p_response)
{
    status_t status;

    if (NULL == p_response)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Send opcode and payload size in one sendall
    uint8_t p_header_buf[5];

    p_header_buf[0] = p_response->status;
    memcpy(p_header_buf + 1, &(p_response->size), sizeof(p_response->size));

    status = sendall(sockfd, p_header_buf, 5u);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    status = sendall(sockfd, p_response->p_payload, ntohl(p_response->size));
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

cleanup:
    return status;
}

static bool
registry_add (registry_t * p_registry, int sockfd)
{
    bool b_added = false;

    pthread_mutex_lock(&(p_registry->lock));
    for (size_t index = 0u; index < max_clients; index++)
    {
        if (-1 == (p_registry->sockfds)[index])
        {
            (p_registry->sockfds)[index] = sockfd;
            (p_registry->count)++;
            b_added = true;
            break;
        }
    }
    pthread_mutex_unlock(&(p_registry->lock));

    return b_added;
}

static bool
registry_remove (registry_t * p_registry, int sockfd)
{
    bool b_removed = false;

    pthread_mutex_lock(&(p_registry->lock));
    for (size_t index = 0u; index < max_clients; index++)
    {
        if (sockfd == (p_registry->sockfds)[index])
        {
            (p_registry->sockfds)[index] = -1;
            (p_registry->count)--;
            b_removed = true;
            break;
        }
    }
    pthread_mutex_unlock(&(p_registry->lock));

    return b_removed;
}

static void
handle_client_wrapper (void * p_arg)
{
    session_t * p_session = p_arg;

    // Handle client session
    handle_client(p_session);

    // Unregister client file descriptor
    if (!registry_remove(p_session->p_registry, p_session->client_fd))
    {
        fprintf(stderr, "client_fd %d not found in registry\n", p_session->client_fd);
    }

    if (-1 == close(p_session->client_fd))
    {
        perror("close");
    }

    free(p_session);
    p_session = NULL;

    return;
}

/*** end of file ***/
