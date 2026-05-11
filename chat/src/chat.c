/** @file chat.c
 *
 * @brief Chat server source
 *
 * @par
 *
 */

#include "chat.h"

int const      default_backlog  = SOMAXCONN;
uint16_t const default_lport    = 3333u;
uint32_t const max_payload_size = 4096u;
uint32_t const chunk_size       = 512u;
uint32_t const max_clients      = 100u;
uint32_t const worker_threads   = 8u;
uint32_t const epoll_max_events = 64u;

/*!
 * @brief Create application data
 *
 * @param[in] capacity Current number of buckets
 *
 * @return Pointer to application data
 */
static appdata_t * appdata_create(size_t capacity);

/*!
 * @brief Destroy application data
 *
 * @param[in] p_appdata Pointer to application data
 *
 * @return Status of operation
 */
static status_t appdata_destroy(appdata_t * p_appdata);

/*!
 * @brief Handle request and generate response
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
static status_t handle_request(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

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

server_t *
chat_server_create (server_t * p_hints, size_t capacity)
{
    status_t status = STATUS_SUCCESS;

    server_t * p_server = NULL;

    if (NULL == p_hints)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_server = server_create(p_hints);
    if (NULL == p_server)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_server->p_appdata = appdata_create(capacity);
    if (NULL == p_server->p_appdata)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

cleanup:
    if (STATUS_SUCCESS != status)
    {
        chat_server_destroy(p_server);
        p_server = NULL;
    }
    return p_server;
}

status_t
chat_server_destroy (server_t * p_server)
{
    status_t status = STATUS_SUCCESS;

    appdata_t * p_appdata = NULL;

    if (NULL == p_server)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_appdata = p_server->p_appdata;

    server_destroy(p_server);
    p_server = NULL;

    appdata_destroy(p_appdata);
    p_appdata = NULL;

cleanup:
    return status;
}

status_t
chat_client_run (server_t * p_server, client_t * p_client)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_server) || (NULL == p_client))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    chat_client_state_t * p_state    = p_client->p_clientdata;
    session_t           * p_session  = &(p_state->session);
    request_t           * p_request  = &(p_state->request);
    response_t          * p_response = &(p_state->response);

    int sockfd = p_client->sockfd;

    memset(p_request->p_payload,  0, max_payload_size);
    memset(p_response->p_payload, 0, max_payload_size);

    status = recv_request(sockfd, p_request);
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "recv_request failed\n");
        goto cleanup;
    }

    if (p_server->b_verbose)
    {
        printf(
            "========================================\n"
            "Request from sockfd %d:\n",
            p_client->sockfd
        );
        display_request(p_request);
    }

    status = handle_request(p_session, p_request, p_response);
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "handle_request failed\n");
        goto cleanup;
    }

    if (p_server->b_verbose)
    {
        printf(
            "========================================\n"
            "Response to sockfd %d:\n",
            p_client->sockfd
        );
        display_response(p_response);
    }

    status = send_response(sockfd, p_response);
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "send_response failed\n");
        goto cleanup;
    }

    if (OPCODE_QUIT == p_request->opcode)
    {
        // Signal server layer to close the connection
        status = STATUS_CLIENT_DISCONNECT;
        goto cleanup;
    }

cleanup:
    return status;
}

status_t
chat_client_init (server_t * p_server, client_t * p_client)
{
    status_t status = STATUS_SUCCESS;
    UNUSED(p_server);

    chat_client_state_t * p_state = NULL;

    if (NULL == p_client)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_state = malloc(sizeof(*p_state));
    if (NULL == p_state)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_client->p_clientdata = p_state;

    memset(p_state, 0, sizeof(*p_state));

    p_state->request.p_payload = malloc(max_payload_size);
    if (NULL == p_state->request.p_payload)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_state->response.p_payload = malloc(max_payload_size);
    if (NULL == p_state->response.p_payload)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_state->session.p_server = p_server;

cleanup:
    if (STATUS_SUCCESS != status)
    {
        chat_client_free(p_server, p_client);
    }
    return status;
}

status_t
chat_client_free (server_t * p_server, client_t * p_client)
{
    status_t status = STATUS_SUCCESS;
    UNUSED(p_server);

    if (NULL == p_client)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    chat_client_state_t * p_state = p_client->p_clientdata;
    if (NULL == p_state)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    opcode_logout(&(p_state->session), NULL, NULL);

    free(p_state->request.p_payload);
    p_state->request.p_payload = NULL;

    free(p_state->response.p_payload);
    p_state->response.p_payload = NULL;

    free(p_state);
    p_state = NULL;

    p_client->p_clientdata = NULL;

cleanup:
    return status;
}

static appdata_t *
appdata_create (size_t capacity)
{
    status_t status = STATUS_SUCCESS;

    appdata_t     * p_appdata       = NULL;
    opcode_func_t * pp_opcode_funcs = NULL;

    p_appdata = malloc(sizeof(*p_appdata));
    if (NULL == p_appdata)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memset(p_appdata, 0, sizeof(*p_appdata));

    p_appdata->p_cred_store = safe_ht_create(capacity);
    if (NULL == p_appdata->p_cred_store)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    pp_opcode_funcs = calloc(UINT8_MAX + 1u, sizeof(*pp_opcode_funcs));
    if (NULL == pp_opcode_funcs)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    pp_opcode_funcs[OPCODE_DEFAULT] = opcode_default;
    pp_opcode_funcs[OPCODE_PING]    = opcode_ping;
    pp_opcode_funcs[OPCODE_ECHO]    = opcode_echo;
    pp_opcode_funcs[OPCODE_QUIT]    = opcode_quit;
    pp_opcode_funcs[OPCODE_LOGIN]   = opcode_login;
    pp_opcode_funcs[OPCODE_LOGOUT]  = opcode_logout;

    p_appdata->pp_opcode_funcs = pp_opcode_funcs;

cleanup:
    if (STATUS_SUCCESS != status)
    {
        appdata_destroy(p_appdata);
        p_appdata = NULL;
    }

    return p_appdata;
}

static status_t
appdata_destroy (appdata_t * p_appdata)
{
    status_t status = STATUS_SUCCESS;

    safe_ht_t     * p_cred_store    = NULL;
    opcode_func_t * pp_opcode_funcs = NULL;

    if (NULL == p_appdata)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_cred_store    = p_appdata->p_cred_store;
    pp_opcode_funcs = p_appdata->pp_opcode_funcs;

    free(p_appdata);
    p_appdata = NULL;

    safe_ht_destroy(p_cred_store);
    p_cred_store = NULL;

    free(pp_opcode_funcs);
    pp_opcode_funcs = NULL;

cleanup:
    return status;
}

static status_t
handle_request (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t     * p_appdata       = NULL;
    opcode_func_t * pp_opcode_funcs = NULL;
    opcode_func_t   p_opcode_func   = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if ((0u == p_session->session_id) &&
        (OPCODE_LOGIN != p_request->opcode) &&
        (OPCODE_QUIT  != p_request->opcode))
    {
        p_response->status = 0x01;

        status = write_response(p_response, "NOT AUTHENTICATED");
        goto cleanup;
    }

    p_appdata       = p_session->p_server->p_appdata;
    pp_opcode_funcs = p_appdata->pp_opcode_funcs;

    p_opcode_func = pp_opcode_funcs[p_request->opcode];
    if (NULL == p_opcode_func)
    {
        p_opcode_func = pp_opcode_funcs[OPCODE_DEFAULT];

        if (NULL == p_opcode_func)
        {
            fprintf(stderr, "Default opcode function doesn't exist\n");
            status = STATUS_NULL_ARG;
            goto cleanup;
        }
    }

    status = p_opcode_func(p_session, p_request, p_response);

cleanup:
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

    display_hex(p_request->p_payload, host_request_size, " ", "\n");
    printf("    string : ");
    display_unicode(p_request->p_payload, host_request_size, "", "\n");
    printf("}\n");

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

    display_hex(p_response->p_payload, host_response_size, " ", "\n");
    printf("    string : ");
    display_unicode(p_response->p_payload, host_response_size, "", "\n");
    printf("}\n");

cleanup:
    return;
}

static status_t
recv_request (int sockfd, request_t * p_request)
{
    status_t  status     = STATUS_SUCCESS;
    uint8_t * p_recv_buf = NULL;

    if (NULL == p_request)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    uint32_t const header_size = 5u;

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
    memcpy(&(p_request->size),   p_recv_buf + 1, sizeof(p_request->size));

    uint32_t const payload_size = ntohl(p_request->size);

    if (payload_size > max_payload_size)
    {
        fprintf(
            stderr,
            "Payload size %u exceeds max_payload_size %u; draining\n",
            payload_size,
            max_payload_size
        );

        sockutil_drain(sockfd, payload_size, chunk_size);

        status = STATUS_OVERFLOW;
        goto cleanup;
    }

    uint32_t const packet_size = header_size + payload_size;

    // Allocate temporary pointer in case of realloc failure
    uint8_t * p_tmp = realloc(p_recv_buf, packet_size);
    if (NULL == p_tmp)
    {
        fprintf(stderr, "realloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_recv_buf = p_tmp;

    // Receive payload
    // NOTE: Enforces payload is exactly payload_size bytes
    status = sockutil_recvall(sockfd, p_recv_buf + header_size, payload_size);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    memcpy(p_request->p_payload, p_recv_buf + header_size, payload_size);

cleanup:
    free(p_recv_buf);
    p_recv_buf = NULL;
    return status;
}

static status_t
send_response (int sockfd, response_t * p_response)
{
    status_t  status     = STATUS_SUCCESS;
    uint8_t * p_send_buf = NULL;

    if (NULL == p_response)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    uint32_t const header_size  = 5u;
    uint32_t const payload_size = ntohl(p_response->size);
    uint32_t const packet_size  = header_size + payload_size;

    p_send_buf = malloc(packet_size);
    if (NULL == p_send_buf)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memcpy(p_send_buf + 0, &(p_response->status), sizeof(p_response->status));
    memcpy(p_send_buf + 1, &(p_response->size),   sizeof(p_response->size));
    memcpy(p_send_buf + 5, p_response->p_payload, payload_size);

    status = sockutil_sendall(sockfd, p_send_buf, packet_size);

cleanup:
    free(p_send_buf);
    p_send_buf = NULL;
    return status;
}

/*** end of file ***/
