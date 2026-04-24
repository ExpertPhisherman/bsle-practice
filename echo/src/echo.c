/** @file echo.c
 *
 * @brief Echo server
 *
 * @par
 *
 */

#include "echo.h"

extern _Atomic sig_atomic_t g_keep_running;

uint16_t const default_lport = 4444u;
int const default_backlog = 10;
uint32_t const max_payload_size = 4096u;
uint32_t const chunk_size = 512u;
uint32_t const max_clients = 1000u;
uint32_t const worker_threads = 8u;

/*!
 * @brief Run client data recv/send loop
 *
 * @param[in] p_server Pointer to server
 * @param[in] p_client Pointer to client
 *
 * @return Status of operation
 */
static status_t client_run(server_t * p_server, client_t * p_client);

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

status_t
echo_load_app (server_t * p_server)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_server)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_server->p_client_run = client_run;

cleanup:
    return status;
}

static status_t
client_run (server_t * p_server, client_t * p_client)
{
    status_t status = STATUS_SUCCESS;

    request_t request;
    response_t response;

    request.p_payload = NULL;
    response.p_payload = NULL;

    if ((NULL == p_server) || (NULL == p_client))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    int sockfd = p_client->sockfd;

    request.opcode = 0x00;
    request.size = 0u;
    request.p_payload = malloc(max_payload_size);
    if (NULL == request.p_payload)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    response.status = 0x00;
    response.size = 0u;
    response.p_payload = malloc(max_payload_size);
    if (NULL == response.p_payload)
    {
        fprintf(stderr, "malloc failed\n");
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

            if (p_server->b_verbose)
            {
                printf(
                    "========================================\n"
                    "Response to sockfd %d:\n", p_client->sockfd
                );
                (display_response)(&response);
            }

            status = send_response(sockfd, &response);
            if (STATUS_SUCCESS != status)
            {
                goto cleanup;
            }

            continue;
        }
        else if (STATUS_CLIENT_DISCONNECT == status)
        {
            fprintf(
                stderr,
                "Abrupt disconnect from client %s:%hu (sockfd %d)\n",
                p_client->p_rhost,
                p_client->rport,
                sockfd
            );
            goto cleanup;
        }
        else if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }
        else
        {
            // Pass
        }

        if (p_server->b_verbose)
        {
            printf(
                "========================================\n"
                "Request from sockfd %d:\n", p_client->sockfd
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

        if (p_server->b_verbose)
        {
            printf(
                "========================================\n"
                "Response to sockfd %d:\n", p_client->sockfd
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
            printf(
                "Graceful disconnect from client %s:%hu (sockfd %d)\n",
                p_client->p_rhost,
                p_client->rport,
                sockfd
            );
            status = STATUS_SUCCESS;
            goto cleanup;
        }
    }

    status = STATUS_SERVER_DISCONNECT;

cleanup:
    free(request.p_payload);
    request.p_payload = NULL;
    free(response.p_payload);
    response.p_payload = NULL;

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
    uint8_t * p_recv_buf = NULL;

    if (NULL == p_request)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    uint32_t header_size = 5u;

    p_recv_buf = malloc(header_size);
    if (NULL == p_recv_buf)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Receive opcode and payload size in one sockutil_recvall
    status = sockutil_recvall(sockfd, p_recv_buf, header_size);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    memcpy(&(p_request->opcode), p_recv_buf + 0, sizeof(p_request->opcode));
    memcpy(&(p_request->size), p_recv_buf + 1, sizeof(p_request->size));

    uint32_t payload_size = ntohl(p_request->size);
    uint32_t packet_size = header_size + payload_size;

    // Reject oversized payloads
    if (payload_size > max_payload_size)
    {
        sockutil_drain(sockfd, payload_size, chunk_size);

        status = STATUS_OVERFLOW;
        goto cleanup;
    }

    p_recv_buf = realloc(p_recv_buf, packet_size);
    if (NULL == p_recv_buf)
    {
        fprintf(stderr, "realloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Receive payload
    // NOTE: Enforces payload is exactly payload_size bytes
    status = sockutil_recvall(sockfd, p_recv_buf + 5, payload_size);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    memcpy(p_request->p_payload, p_recv_buf + 5, payload_size);

cleanup:
    free(p_recv_buf);
    p_recv_buf = NULL;
    return status;
}

static status_t
send_response (int sockfd, response_t * p_response)
{
    status_t status = STATUS_SUCCESS;
    uint8_t * p_send_buf = NULL;

    if (NULL == p_response)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    uint32_t header_size = 5u;
    uint32_t payload_size = ntohl(p_response->size);
    uint32_t packet_size = header_size + payload_size;

    p_send_buf = malloc(packet_size);
    if (NULL == p_send_buf)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memcpy(p_send_buf + 0, &(p_response->status), sizeof(p_response->status));
    memcpy(p_send_buf + 1, &(p_response->size), sizeof(p_response->size));
    memcpy(p_send_buf + 5, p_response->p_payload, payload_size);

    // Send opcode, payload size, and payload in one sockutil_sendall
    status = sockutil_sendall(sockfd, p_send_buf, packet_size);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

cleanup:
    free(p_send_buf);
    p_send_buf = NULL;
    return status;
}

/*** end of file ***/
