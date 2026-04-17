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
_Atomic sig_atomic_t g_keep_running = 1;

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
 * @brief Display request
 *
 * @param[in] p_request Pointer to request
 *
 * @return void
 */
static void display_request(request_t * p_request);

/*!
 * @brief Display response
 *
 * @param[in] p_response Pointer to response
 *
 * @return void
 */
static void display_response(response_t * p_response);

/*!
 * @brief Receive request from client
 *
 * @param[in] sockfd    Socket file descriptor
 * @param[in] p_request Pointer to request
 *
 * @return Status of operation
 */
static status_t recv_request(int sockfd, request_t * p_request);

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

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        perror("socket");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (-1 == setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
    {
        perror("setsockopt");
        if (-1 == close(sockfd))
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
        if (-1 == close(sockfd))
        {
            perror("close");
        }

        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (-1 == bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
    {
        perror("bind");
        if (-1 == close(sockfd))
        {
            perror("close");
        }

        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (-1 == listen(sockfd, p_session->backlog))
    {
        perror("listen");
        if (-1 == close(sockfd))
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
        printf("Listening on %s:%hu (sockfd %d)\n", p_ipstr, p_session->lport, sockfd);
    }

    p_session->sockfd = sockfd;
    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    return status;
}

status_t
client_sock (session_t * p_session)
{
    status_t status;
    int sockfd;

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
        sockfd = accept(p_session->sockfd, (struct sockaddr *)&client_addr, &sin_size);
        if (-1 == sockfd)
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
            printf("Accepted connection from %s:%hu (sockfd %d)\n", p_ipstr, rport, sockfd);
        }

        // Multithread client socket connections
        session_t * p_client_session = malloc(sizeof(*p_client_session));
        if (NULL == p_client_session)
        {
            fprintf(stderr, "malloc failed for sockfd %d\n", sockfd);
            if (-1 == close(sockfd))
            {
                perror("close");
            }
            continue;
        }

        *p_client_session = *p_session;
        p_client_session->rport = rport;
        p_client_session->sockfd = sockfd;

        // Register client file descriptor
        if (!registry_add(p_session->p_registry, sockfd))
        {
            fprintf(stderr, "max_clients reached, closing sockfd %d\n", sockfd);
            if (-1 == close(sockfd))
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
            if (!registry_remove(p_session->p_registry, sockfd))
            {
                fprintf(stderr, "sockfd %d not found in registry\n", sockfd);
            }
            if (-1 == close(sockfd))
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
        printf("\nGraceful shutdown on server (sockfd %d)\n", p_session->sockfd);
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

    if (NULL == p_session)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    int sockfd = p_session->sockfd;
    request_t request;
    response_t response;

    request.opcode = 0x00;
    request.size = 0u;
    request.p_payload = malloc(max_payload_size);
    if (NULL == request.p_payload)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    response.status = 0x00;
    response.size = 0u;
    response.p_payload = malloc(max_payload_size);
    if (NULL == response.p_payload)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    while (g_keep_running)
    {
        memset(request.p_payload, 0, max_payload_size);
        memset(response.p_payload, 0, max_payload_size);

        status = recv_request(sockfd, &request);
        if (STATUS_OVERFLOW == status)
        {
            response.status = 0x01;
            response.size = htonl((uint32_t)snprintf(
                response.p_payload, max_payload_size,
                "Request payload size %u exceeds max_payload_size %u",
                ntohl(request.size), max_payload_size
            ));
            fprintf(stderr, "%s\n", response.p_payload);

            if (p_session->b_verbose)
            {
                printf(
                    "========================================\n"
                    "Response to sockfd %d:\n", p_session->sockfd
                );
                display_response(&response);
            }

            status = send_response(sockfd, &response);
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

        if (p_session->b_verbose)
        {
            printf(
                "========================================\n"
                "Request from sockfd %d:\n", p_session->sockfd
            );
            display_request(&request);
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
                fprintf(stderr, "Unknown opcode from sockfd %d: 0x%02hhx\n", sockfd, request.opcode);
                break;
        }

        uint32_t host_response_size = strnlen(p_response_payload, max_payload_size);
        response.size = htonl(host_response_size);
        memcpy(response.p_payload, p_response_payload, host_response_size);

        if (p_session->b_verbose)
        {
                printf(
                    "========================================\n"
                    "Response to sockfd %d:\n", p_session->sockfd
                );
                display_response(&response);
        }

        status = send_response(sockfd, &response);
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }

        if (OPCODE_QUIT == request.opcode)
        {
            // Close connection
            status = STATUS_SUCCESS;
            goto cleanup;
        }
    }

    status = STATUS_SERVER_DISCONNECT;
    goto cleanup;

cleanup:
    free(request.p_payload);
    request.p_payload = NULL;
    free(response.p_payload);
    response.p_payload = NULL;

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
            if (EINTR == errno)
            {
                if (!g_keep_running)
                {
                    status = STATUS_SERVER_DISCONNECT;
                    goto cleanup;
                }
                continue;
            }

            status = STATUS_RECV_FAILURE;
            goto cleanup;
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
        fprintf(stderr, "Abrupt disconnect from client (sockfd %d)\n", sockfd);
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
    if (NULL == p_buf)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

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

static void
display_request (request_t * p_request)
{
    if (NULL == p_request)
    {
        goto cleanup;
    }

    uint32_t host_request_size = ntohl(p_request->size);

    printf(
        "{\n"
        "    opcode : 0x%02hhx\n"
        "    size   : %u\n"
        "    payload: ",
        p_request->opcode,
        host_request_size
    );

    display_bytes(p_request->p_payload, host_request_size, " ");
    printf("\n}\n");

cleanup:
    return;
}

static void
display_response (response_t * p_response)
{
    if (NULL == p_response)
    {
        goto cleanup;
    }

    uint32_t host_response_size = ntohl(p_response->size);

    printf(
        "{\n"
        "    status : 0x%02hhx\n"
        "    size   : %u\n"
        "    payload: ",
        p_response->status,
        host_response_size
    );

    display_bytes(p_response->p_payload, host_response_size, " ");
    printf("\n}\n");

cleanup:
    return;
}

static status_t
recv_request (int sockfd, request_t * p_request)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_request)
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
        drain(sockfd, host_request_size);

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
    status_t status = STATUS_SUCCESS;

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
        if (-1 == (p_registry->p_sockfds)[index])
        {
            (p_registry->p_sockfds)[index] = sockfd;
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
        if (sockfd == (p_registry->p_sockfds)[index])
        {
            (p_registry->p_sockfds)[index] = -1;
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
    if (!registry_remove(p_session->p_registry, p_session->sockfd))
    {
        fprintf(stderr, "sockfd %d not found in registry\n", p_session->sockfd);
    }

    if (-1 == close(p_session->sockfd))
    {
        perror("close");
    }

    free(p_session);
    p_session = NULL;

    return;
}

/*** end of file ***/
