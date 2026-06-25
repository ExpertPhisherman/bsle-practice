/** @file chat.c
 *
 * @brief Chat server source
 *
 * @par
 *
 */

#include "chat.h"

uint16_t const g_default_lport          = 3333u;
uint32_t const g_max_packet_size        = 4096u;
uint32_t const g_chunk_size             = 512u;
uint16_t const g_file_chunk_size        = 512u;
size_t const   g_creds_capacity         = 17u;
size_t const   g_admins_capacity        = 17u;
size_t const   g_session_store_capacity = 17u;
size_t const   g_pm_reqs_capacity       = 17u;
size_t const   g_file_reqs_capacity     = 17u;
uint16_t const g_username_size_min      = 3u;
uint16_t const g_username_size_max      = 16u;
uint16_t const g_password_size_min      = 8u;
uint16_t const g_password_size_max      = 128u;

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
 * @brief Seed credential store with default admin credentials
 *
 * @param[in,out] p_cred_store Pointer to credential storage
 *
 * @return Status of operation
 */
static status_t appdata_seed_creds(ht_t * p_cred_store);

/*!
 * @brief Seed room store with default rooms and configure callbacks
 *
 * @param[in,out] p_room_store Pointer to room storage
 *
 * @return Status of operation
 */
static status_t appdata_seed_rooms(sll_t * p_room_store);

/*!
 * @brief Seed admin table with default admin username
 *
 * @param[in,out] p_admins Pointer to admin table
 *
 * @return Status of operation
 */
static status_t appdata_seed_admins(ht_t * p_admins);

/*!
 * @brief Populate opcode function dispatch table
 *
 * @param[in,out] pp_opcode_funcs Pointer to opcode function array
 *
 * @return Status of operation
 */
static void appdata_init_opcodes(opcode_func_t * pp_opcode_funcs);

/*!
 * @brief Compare room
 *
 * @param[in] p_data1 Pointer to room
 * @param[in] p_data2 Pointer to room name
 * @param[in] size    Size of room name in bytes
 *
 * @return Difference between rooms
 */
static int compare_room(
    void const * p_data1,
    void const * p_data2,
    size_t       size
);

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
    p_request->size   = sizeof(p_request->opcode);
    memset(p_request->p_packet, 0, g_max_packet_size);

    sockutil_recvall(sockfd, p_request->p_packet, p_request->size);
    p_request->opcode = (p_request->p_packet)[FIELD_OFFSET_OPCODE];

    p_response->opcode  = p_request->opcode;
    p_response->retcode = RETCODE_SUCCESS;
    p_response->size    = (
        sizeof(p_response->opcode) +
        sizeof(p_response->retcode)
    );
    memset(p_response->p_packet, 0, g_max_packet_size);

    status = handle_request(p_session, p_request, p_response);
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "handle_request failed\n");
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

    state_t * p_state           = NULL;
    uint8_t * p_request_packet  = NULL;
    uint8_t * p_response_packet = NULL;
    uint8_t * p_username        = NULL;
    uint8_t * p_password        = NULL;
    uint8_t * p_user_allow      = NULL;

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

    p_state->session.p_server  = p_server;
    p_state->session.p_client  = p_client;
    p_state->session.sockfd    = p_client->sockfd;
    p_state->request.p_packet  = NULL;
    p_state->response.p_packet = NULL;

    p_state->session.username_size   = 0u;
    p_state->session.password_size   = 0u;
    p_state->session.user_allow_size = 0u;

    p_username = calloc(g_username_size_max, sizeof(*p_username));
    if (NULL == p_username)
    {
        fprintf(stderr, "calloc failed in chat_client_init\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_state->session.p_username = p_username;

    p_password = calloc(g_password_size_max, sizeof(*p_password));
    if (NULL == p_password)
    {
        fprintf(stderr, "calloc failed in chat_client_init\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_state->session.p_password = p_password;

    p_request_packet = calloc(g_max_packet_size, sizeof(*p_request_packet));
    if (NULL == p_request_packet)
    {
        fprintf(stderr, "calloc failed in chat_client_init\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_state->request.p_packet = p_request_packet;

    p_response_packet = calloc(g_max_packet_size, sizeof(*p_response_packet));
    if (NULL == p_response_packet)
    {
        fprintf(stderr, "calloc failed in chat_client_init\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_state->response.p_packet = p_response_packet;

    p_user_allow = calloc(g_username_size_max, sizeof(*p_user_allow));
    if (NULL == p_user_allow)
    {
        fprintf(stderr, "calloc failed in chat_client_init\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_state->session.p_user_allow = p_user_allow;

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
    bool         b_locked     = false;

    if (
        (NULL == p_server) ||
        (NULL == p_client) ||
        (NULL == p_client->p_clientdata)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_state = p_client->p_clientdata;

    p_session  = &(p_state->session);
    p_request  = &(p_state->request);
    p_response = &(p_state->response);

    p_appdata = p_server->p_appdata;
    if (NULL == p_appdata)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    free(p_request->p_packet);
    p_request->p_packet = NULL;

    free(p_response->p_packet);
    p_response->p_packet = NULL;

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    user_logout(p_session, p_appdata);

    free(p_session->p_username);
    p_session->p_username = NULL;

    free(p_session->p_password);
    p_session->p_password = NULL;

    free(p_session->p_user_allow);
    p_session->p_user_allow = NULL;

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

static status_t
appdata_seed_creds (ht_t * p_cred_store)
{
    return ht_set(p_cred_store, "admin", 5u, "password", 8u);
}

static status_t
appdata_seed_rooms (sll_t * p_room_store)
{
    status_t status = STATUS_SUCCESS;

    room_t * p_room = NULL;

    p_room_store->p_destroy_data = room_destroy;
    p_room_store->p_compare_node = compare_room;

    p_room = room_create("general", 7u);
    if (NULL == p_room)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    status = sll_append(p_room_store, &p_room, sizeof(p_room));
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "sll_append failed in appdata_seed_rooms\n");
        room_destroy(p_room);
        p_room = NULL;
    }

cleanup:
    return status;
}

static status_t
appdata_seed_admins (ht_t * p_admins)
{
    return ht_set(p_admins, "admin", 5u, "", 0u);
}

static void
appdata_init_opcodes (opcode_func_t * pp_opcode_funcs)
{
    pp_opcode_funcs[OPCODE_DEFAULT]    = opcode_default;
    pp_opcode_funcs[OPCODE_PING]       = opcode_ping;
    pp_opcode_funcs[OPCODE_ECHO]       = opcode_echo;
    pp_opcode_funcs[OPCODE_QUIT]       = opcode_quit;
    pp_opcode_funcs[OPCODE_LOGIN]      = opcode_login;
    pp_opcode_funcs[OPCODE_LOGOUT]     = opcode_logout;
    pp_opcode_funcs[OPCODE_MSG_SEND]   = opcode_msg_send;
    pp_opcode_funcs[OPCODE_MSG_RECV]   = NULL;
    pp_opcode_funcs[OPCODE_JOIN]       = opcode_join;
    pp_opcode_funcs[OPCODE_LIST]       = opcode_list;
    pp_opcode_funcs[OPCODE_REQUEST]    = opcode_request;
    pp_opcode_funcs[OPCODE_RESPOND]    = opcode_respond;
    pp_opcode_funcs[OPCODE_FILE_SEND]  = opcode_file_send;
    pp_opcode_funcs[OPCODE_PROMOTE]    = opcode_promote;
    pp_opcode_funcs[OPCODE_DISCONNECT] = opcode_disconnect;
    pp_opcode_funcs[OPCODE_DELETE]     = opcode_delete;
}

static appdata_t *
appdata_create (void)
{
    status_t status = STATUS_SUCCESS;

    appdata_t     * p_appdata        = NULL;
    ht_t          * p_cred_store     = NULL;
    sll_t         * p_room_store     = NULL;
    ht_t          * p_admins         = NULL;
    ht_t          * p_session_store  = NULL;
    ht_t          * p_pm_reqs        = NULL;
    ht_t          * p_file_reqs      = NULL;
    opcode_func_t * pp_opcode_funcs  = NULL;

    p_appdata = calloc(1u, sizeof(*p_appdata));
    if (NULL == p_appdata)
    {
        fprintf(stderr, "calloc failed in appdata_create\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_cred_store = ht_create(g_creds_capacity);
    if (NULL == p_cred_store)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_appdata->p_cred_store = p_cred_store;

    status = appdata_seed_creds(p_cred_store);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    p_room_store = sll_create();
    if (NULL == p_room_store)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_appdata->p_room_store = p_room_store;

    status = appdata_seed_rooms(p_room_store);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    p_admins = ht_create(g_admins_capacity);
    if (NULL == p_admins)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_appdata->p_admins = p_admins;

    status = appdata_seed_admins(p_admins);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    p_session_store = ht_create(g_session_store_capacity);
    if (NULL == p_session_store)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_appdata->p_session_store = p_session_store;

    p_pm_reqs = ht_create(g_pm_reqs_capacity);
    if (NULL == p_pm_reqs)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_appdata->p_pm_reqs = p_pm_reqs;

    p_file_reqs = ht_create(g_file_reqs_capacity);
    if (NULL == p_file_reqs)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_appdata->p_file_reqs = p_file_reqs;

    pp_opcode_funcs = calloc(UINT8_MAX + 1u, sizeof(*pp_opcode_funcs));
    if (NULL == pp_opcode_funcs)
    {
        fprintf(stderr, "calloc failed in appdata_create\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_appdata->pp_opcode_funcs = pp_opcode_funcs;

    appdata_init_opcodes(pp_opcode_funcs);

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

    sll_destroy(p_appdata->p_room_store);
    p_appdata->p_room_store = NULL;

    ht_destroy(p_appdata->p_admins);
    p_appdata->p_admins = NULL;

    ht_destroy(p_appdata->p_session_store);
    p_appdata->p_session_store = NULL;

    ht_destroy(p_appdata->p_pm_reqs);
    p_appdata->p_pm_reqs = NULL;

    ht_destroy(p_appdata->p_file_reqs);
    p_appdata->p_file_reqs = NULL;

    free(p_appdata->pp_opcode_funcs);
    p_appdata->pp_opcode_funcs = NULL;

    pthread_mutex_destroy(&(p_appdata->lock));

    free(p_appdata);
    p_appdata = NULL;

cleanup:
    return status;
}

static int
compare_room (
    void const * p_data1,
    void const * p_data2,
    size_t size
)
{
    int result = 0;

    room_t  * p_room      = *(room_t  **)p_data1;
    uint8_t * p_room_name =  (uint8_t  *)p_data2;

    if (((NULL == p_room) || (NULL == p_room->p_name)) && (NULL == p_room_name))
    {
        goto cleanup;
    }

    if ((NULL == p_room) || (NULL == p_room->p_name))
    {
        result = -1;
        goto cleanup;
    }

    if (NULL == p_room_name)
    {
        result = 1;
        goto cleanup;
    }

    if (p_room->name_size != size)
    {
        result = (p_room->name_size > size) ? 1 : -1;
        goto cleanup;
    }

    result = memcmp(p_room->p_name, p_room_name, size);

cleanup:
    return result;
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
