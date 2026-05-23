/** @file chat.c
 *
 * @brief Chat server source
 *
 * @par
 *
 */

#include "chat.h"

uint16_t const g_default_lport   = 3333u;
uint32_t const g_max_packet_size = 4096u;
uint32_t const g_chunk_size      = 512u;
size_t const   g_creds_capacity  = 17u;
size_t const   g_rooms_capacity  = 17u;

/*!
 * @brief Create application data
 *
 * @param[in] void
 *
 * @return Pointer to application data
 */
static appdata_t * appdata_create(void);

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

server_t *
chat_server_create (server_t * p_hints)
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

    p_server->p_appdata = appdata_create();
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

    state_t * p_state = p_client->p_clientdata;
    if (NULL == p_state)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    session_t  * p_session  = &(p_state->session);
    request_t  * p_request  = &(p_state->request);
    response_t * p_response = &(p_state->response);

    int sockfd = p_client->sockfd;

    p_request->opcode = OPCODE_DEFAULT;
    p_request->size   = FIELD_SIZE_OPCODE;
    memset(p_request->p_packet, 0, g_max_packet_size);

    p_response->opcode  = OPCODE_DEFAULT;
    p_response->retcode = RETCODE_FAILURE;
    p_response->size    = FIELD_SIZE_OPCODE + FIELD_SIZE_RETCODE;
    memset(p_response->p_packet, 0, g_max_packet_size);

    sockutil_recvall(sockfd, p_request->p_packet, FIELD_SIZE_OPCODE);
    p_request->opcode = (p_request->p_packet)[FIELD_OFFSET_OPCODE];

    status = handle_request(p_session, p_request, p_response);
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "handle_request failed\n");
        goto cleanup;
    }

    (p_response->p_packet)[FIELD_OFFSET_OPCODE]  = p_response->opcode;
    (p_response->p_packet)[FIELD_OFFSET_RETCODE] = p_response->retcode;

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

    state_t * p_state = NULL;

    if ((NULL == p_server) || (NULL == p_client))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_state = calloc(1u, sizeof(*p_state));
    if (NULL == p_state)
    {
        fprintf(stderr, "calloc failed in chat_client_init\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_client->p_clientdata = p_state;

    p_state->session.p_server = p_server;
    p_state->session.p_client = p_client;
    p_state->session.sockfd   = p_client->sockfd;

    p_state->request.p_packet = calloc(1u, g_max_packet_size);
    if (NULL == p_state->request.p_packet)
    {
        fprintf(stderr, "calloc failed in chat_client_init\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_state->response.p_packet = calloc(1u, g_max_packet_size);
    if (NULL == p_state->response.p_packet)
    {
        fprintf(stderr, "calloc failed in chat_client_init\n");
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

    state_t    * p_state      = NULL;
    session_t  * p_session    = NULL;
    request_t  * p_request    = NULL;
    response_t * p_response   = NULL;
    appdata_t  * p_appdata    = NULL;
    ht_t       * p_room_store = NULL;
    item_t     * p_item       = NULL;
    room_t     * p_room       = NULL;
    bool         b_locked     = false;

    if ((NULL == p_server) || (NULL == p_client))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_state = p_client->p_clientdata;
    if (NULL == p_state)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_session  = &(p_state->session);
    p_request  = &(p_state->request);
    p_response = &(p_state->response);

    p_appdata = p_server->p_appdata;
    if (NULL == p_appdata)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_room_store = p_appdata->p_room_store;
    if (NULL == p_room_store)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    free(p_session->p_username);
    p_session->p_username = NULL;

    free(p_session->p_password);
    p_session->p_password = NULL;

    free(p_request->p_packet);
    p_request->p_packet = NULL;

    free(p_response->p_packet);
    p_response->p_packet = NULL;

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    // Remove p_session from room's sessions SLL
    p_item = ht_get(
        p_room_store,
        p_session->p_room_name,
        p_session->room_name_size
    );
    if (NULL != p_item)
    {
        p_room = p_item->p_value;
        sll_remove(p_room->p_sessions, &p_session, sizeof(p_session));
    }

    free(p_session->p_room_name);
    p_session->p_room_name = NULL;

    free(p_state);
    p_state = NULL;

    p_client->p_clientdata = NULL;

cleanup:
    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
    }
    return status;
}

room_t *
room_create (char * p_name, uint16_t name_size)
{
    status_t status = STATUS_SUCCESS;

    room_t * p_room     = NULL;
    sll_t  * p_sessions = NULL;

    p_room = malloc(sizeof(*p_room));
    if (NULL == p_room)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memset(p_room, 0, sizeof(*p_room));

    p_room->name_size = name_size;

    p_room->p_name = malloc(name_size);
    if (NULL == p_room->p_name)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memcpy(p_room->p_name, p_name, name_size);

    p_sessions = sll_create();
    if (NULL == p_sessions)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_room->p_sessions = p_sessions;

cleanup:
    if (STATUS_SUCCESS != status)
    {
        room_destroy(p_room);
        p_room = NULL;
    }

    return p_room;
}

void
room_destroy (void * p_data)
{
    if (NULL == p_data)
    {
        goto cleanup;
    }

    room_t * p_room = p_data;
    p_data = NULL;

    free(p_room->p_name);
    p_room->p_name = NULL;

    sll_destroy(p_room->p_sessions);
    p_room->p_sessions = NULL;

    free(p_room);
    p_room = NULL;

cleanup:
    return;
}

static appdata_t *
appdata_create (void)
{
    status_t status = STATUS_SUCCESS;

    appdata_t     * p_appdata         = NULL;
    ht_t          * p_cred_store      = NULL;
    ht_t          * p_room_store      = NULL;
    room_t        * p_room            = NULL;
    opcode_func_t * pp_opcode_funcs   = NULL;

    p_appdata = malloc(sizeof(*p_appdata));
    if (NULL == p_appdata)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memset(p_appdata, 0, sizeof(*p_appdata));

    p_cred_store = ht_create(g_creds_capacity);
    if (NULL == p_cred_store)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_appdata->p_cred_store = p_cred_store;

    status = ht_set(p_cred_store, (char *)"admin", 5u, (char *)"password", 8u);
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "ht_set failed\n");
        goto cleanup;
    }

    p_room_store = ht_create(g_rooms_capacity);
    if (NULL == p_room_store)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_appdata->p_room_store = p_room_store;

    p_room_store->p_destroy_value = room_destroy;

    p_room = room_create((char *)"general", 7u);
    if (NULL == p_room)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    status = ht_set(
        p_room_store,
        p_room->p_name,
        p_room->name_size,
        p_room,
        sizeof(*p_room)
    );
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "ht_set failed\n");

        room_destroy(p_room);
        p_room = NULL;
        goto cleanup;
    }

    // Free room after hash table makes its own copy
    free(p_room);
    p_room = NULL;

    pp_opcode_funcs = calloc(UINT8_MAX + 1u, sizeof(*pp_opcode_funcs));
    if (NULL == pp_opcode_funcs)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_appdata->pp_opcode_funcs = pp_opcode_funcs;

    pp_opcode_funcs[OPCODE_DEFAULT]  = opcode_default;
    pp_opcode_funcs[OPCODE_PING]     = opcode_ping;
    pp_opcode_funcs[OPCODE_ECHO]     = opcode_echo;
    pp_opcode_funcs[OPCODE_QUIT]     = opcode_quit;
    pp_opcode_funcs[OPCODE_LOGIN]    = opcode_login;
    pp_opcode_funcs[OPCODE_LOGOUT]   = opcode_logout;
    pp_opcode_funcs[OPCODE_MSG_SEND] = opcode_msg_send;
    pp_opcode_funcs[OPCODE_MSG_RECV] = NULL;
    pp_opcode_funcs[OPCODE_JOIN]     = opcode_join;

    p_appdata->next_session_id = 1u;

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

/*** end of file ***/
