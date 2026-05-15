/** @file chat.c
 *
 * @brief Chat server source
 *
 * @par
 *
 */

#include "chat.h"

int const      default_backlog     = SOMAXCONN;
uint16_t const default_lport       = 3333u;
uint32_t const max_packet_size     = 2048u;
uint32_t const chunk_size          = 512u;
uint32_t const max_clients         = 100u;
uint32_t const worker_threads      = 8u;
uint32_t const epoll_max_events    = 64u;
size_t const   creds_capacity      = 17u;
size_t const   rooms_capacity      = 17u;

/*!
 * @brief Create application data
 *
 * @param[in] creds_capacity Number of buckets in credential storage
 * @param[in] rooms_capacity Number of buckets in room storage
 *
 * @return Pointer to application data
 */
static appdata_t * appdata_create(size_t creds_capacity, size_t rooms_capacity);

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
 * @brief Destroy room
 *
 * @param[in] p_data Pointer to room
 *
 * @return void
 */
static void room_destroy(void * p_data);

server_t *
chat_server_create (
    server_t * p_hints,
    size_t     creds_capacity,
    size_t     rooms_capacity
)
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

    p_server->p_appdata = appdata_create(creds_capacity, rooms_capacity);
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

    p_request->opcode = OPCODE_DEFAULT;
    memset(p_request->p_packet, 0, max_packet_size);

    p_response->opcode  = OPCODE_DEFAULT;
    p_response->retcode = RETCODE_FAILURE;
    memset(p_response->p_packet, 0, max_packet_size);
    p_response->size = 0u;

    sockutil_recvall(sockfd, p_request->p_packet, 1u);
    p_request->opcode = (p_request->p_packet)[0];

    status = handle_request(p_session, p_request, p_response);
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "handle_request failed\n");
        goto cleanup;
    }

    (p_response->p_packet)[0] = p_response->opcode;
    (p_response->p_packet)[1] = p_response->retcode;

    if (p_server->b_verbose)
    {
        // Display request packet
        printf(
            "========================================\n"
            "Request from sockfd %d:\n",
            p_client->sockfd
        );
        display_hex(p_request->p_packet, p_request->size, " ", "\n");

        // Display response packet
        printf(
            "========================================\n"
            "Response to sockfd %d:\n",
            p_client->sockfd
        );
        display_hex(p_response->p_packet, p_response->size, " ", "\n");
    }

    pthread_mutex_lock(&(p_client->lock));
    sockutil_sendall(sockfd, p_response->p_packet, p_response->size);
    pthread_mutex_unlock(&(p_client->lock));

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

    p_state->session.p_server = p_server;
    p_state->session.sockfd   = p_client->sockfd;

    p_state->request.p_packet = malloc(max_packet_size);
    if (NULL == p_state->request.p_packet)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_state->response.p_packet = malloc(max_packet_size);
    if (NULL == p_state->response.p_packet)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

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

    free(p_state->session.p_username);
    p_state->session.p_username = NULL;

    free(p_state->session.p_password);
    p_state->session.p_password = NULL;

    free(p_state->request.p_packet);
    p_state->request.p_packet = NULL;

    free(p_state->response.p_packet);
    p_state->response.p_packet = NULL;

    free(p_state);
    p_state = NULL;

    p_client->p_clientdata = NULL;

cleanup:
    return status;
}

static appdata_t *
appdata_create (size_t creds_capacity, size_t rooms_capacity)
{
    status_t status = STATUS_SUCCESS;

    appdata_t     * p_appdata         = NULL;
    ht_t          * p_cred_store      = NULL;
    ht_t          * p_room_store      = NULL;
    opcode_func_t * pp_opcode_funcs   = NULL;

    p_appdata = malloc(sizeof(*p_appdata));
    if (NULL == p_appdata)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memset(p_appdata, 0, sizeof(*p_appdata));

    p_cred_store = ht_create(creds_capacity);
    if (NULL == p_cred_store)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    status = ht_set(p_cred_store, (void *)"admin", 5u, (void *)"password", 8u);
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "ht_set failed\n");
        goto cleanup;
    }

    p_room_store = ht_create(rooms_capacity);
    if (NULL == p_room_store)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_room_store->p_destroy_value = room_destroy;

    pp_opcode_funcs = calloc(UINT8_MAX + 1u, sizeof(*pp_opcode_funcs));
    if (NULL == pp_opcode_funcs)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    pp_opcode_funcs[OPCODE_DEFAULT]  = opcode_default;
    pp_opcode_funcs[OPCODE_PING]     = opcode_ping;
    pp_opcode_funcs[OPCODE_ECHO]     = opcode_echo;
    pp_opcode_funcs[OPCODE_QUIT]     = opcode_quit;
    pp_opcode_funcs[OPCODE_LOGIN]    = opcode_login;
    pp_opcode_funcs[OPCODE_LOGOUT]   = opcode_logout;
    pp_opcode_funcs[OPCODE_MSG_SEND] = opcode_msg_send;
    pp_opcode_funcs[OPCODE_MSG_RECV] = opcode_msg_recv;

    p_appdata->next_session_id = 1u;
    p_appdata->p_cred_store    = p_cred_store;
    p_appdata->p_room_store    = p_room_store;
    p_appdata->pp_opcode_funcs = pp_opcode_funcs;

    if (0 != pthread_mutex_init(&(p_appdata->lock), NULL))
    {
        perror("pthread_mutex_init");
        status = STATUS_MUTEX_FAILURE;
        goto cleanup;
    }

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

    if (NULL == p_appdata)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    ht_destroy(p_appdata->p_cred_store);
    p_appdata->p_cred_store = NULL;

    ht_destroy(p_appdata->p_room_store);
    p_appdata->p_room_store = NULL;

    free(p_appdata->pp_opcode_funcs);
    p_appdata->pp_opcode_funcs = NULL;

    pthread_mutex_destroy(&(p_appdata->lock));

    free(p_appdata);
    p_appdata = NULL;

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
room_destroy (void * p_data)
{
    if (NULL == p_data)
    {
        goto cleanup;
    }

    room_t * p_room = p_data;

    free(p_room);
    p_room = NULL;

cleanup:
    return;
}

/*** end of file ***/
