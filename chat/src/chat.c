/** @file chat.c
 *
 * @brief Chat server source
 *
 * @par
 *
 */

#include "chat.h"

uint16_t const g_default_lport      = 3333u;
uint32_t const g_max_packet_size    = 4096u;
uint32_t const g_chunk_size         = 512u;
size_t const   g_creds_capacity     = 17u;
size_t const   g_pm_reqs_capacity   = 17u;
size_t const   g_file_reqs_capacity = 17u;
uint16_t const g_username_size_min  = 3u;
uint16_t const g_username_size_max  = 16u;
uint16_t const g_password_size_min  = 8u;
uint16_t const g_password_size_max  = 128u;

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
    size_t size
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

    p_response->opcode  = OPCODE_DEFAULT;
    p_response->retcode = RETCODE_FAILURE;
    p_response->size    = (
        sizeof(p_response->opcode) +
        sizeof(p_response->retcode)
    );
    memset(p_response->p_packet, 0, g_max_packet_size);

    sockutil_recvall(sockfd, p_request->p_packet, p_request->size);
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

    state_t * p_state           = NULL;
    uint8_t * p_request_packet  = NULL;
    uint8_t * p_response_packet = NULL;
    uint8_t * p_username        = NULL;
    uint8_t * p_password        = NULL;

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

    p_state->session.username_size = 0u;
    p_state->session.password_size = 0u;

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
    sll_t      * p_room_store = NULL;
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

    p_room_store = p_appdata->p_room_store;
    if (NULL == p_room_store)
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

    user_leave(p_session, p_appdata);
    user_logout(p_session, p_appdata);

    free(p_session->p_username);
    p_session->p_username = NULL;

    free(p_session->p_password);
    p_session->p_password = NULL;

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
room_create (char const * p_name, uint16_t name_size)
{
    status_t status = STATUS_SUCCESS;

    room_t  * p_room      = NULL;
    sll_t   * p_sessions  = NULL;
    ht_t    * p_pm_reqs   = NULL;
    ht_t    * p_file_reqs = NULL;
    uint8_t * p_plus_chr  = NULL;

    p_room = calloc(1u, sizeof(*p_room));
    if (NULL == p_room)
    {
        fprintf(stderr, "calloc failed in room_create\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_room->name_size = name_size;

    p_room->p_name = calloc(name_size, sizeof(*(p_room->p_name)));
    if (NULL == p_room->p_name)
    {
        fprintf(stderr, "calloc failed in room_create\n");
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

    p_pm_reqs = ht_create(g_pm_reqs_capacity);
    if (NULL == p_pm_reqs)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_room->p_pm_reqs = p_pm_reqs;

    p_file_reqs = ht_create(g_file_reqs_capacity);
    if (NULL == p_file_reqs)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_room->p_file_reqs = p_file_reqs;

    p_room->b_private  = false;
    p_room->p_user1    = NULL;
    p_room->user1_size = 0u;
    p_room->p_user2    = NULL;
    p_room->user2_size = 0u;

    // Check if '+' character is in room name
    p_plus_chr = memchr(p_room->p_name, '+', name_size);

    if (NULL != p_plus_chr)
    {
        p_room->b_private  = true;
        p_room->p_user1    = p_room->p_name;
        p_room->user1_size = p_plus_chr - p_room->p_user1;
        p_room->p_user2    = p_plus_chr + 1u;
        p_room->user2_size = p_room->p_name + name_size - p_room->p_user2;
    }

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

    free(p_room->p_name);
    p_room->p_name = NULL;

    p_room->name_size = 0u;

    sll_destroy(p_room->p_sessions);
    p_room->p_sessions = NULL;

    ht_destroy(p_room->p_pm_reqs);
    p_room->p_pm_reqs = NULL;

    ht_destroy(p_room->p_file_reqs);
    p_room->p_file_reqs = NULL;

    p_room->b_private  = false;
    p_room->p_user1    = NULL;
    p_room->user1_size = 0u;
    p_room->p_user2    = NULL;
    p_room->user2_size = 0u;

    free(p_room);
    p_room = NULL;

cleanup:
    return;
}

uint32_t
user_login (session_t * p_session, appdata_t * p_appdata)
{
    uint32_t session_id = 0u;

    server_t * p_server      = NULL;
    ht_t     * p_cred_store  = NULL;
    item_t   * p_item        = NULL;
    uint16_t   username_size = 0u;
    uint16_t   password_size = 0u;
    uint8_t  * p_username    = NULL;
    uint8_t  * p_password    = NULL;

    if (
        (NULL == p_session) ||
        (NULL == p_appdata) ||
        (NULL == p_appdata->p_cred_store)
    )
    {
        goto cleanup;
    }

    p_server      = p_session->p_server;
    p_cred_store  = p_appdata->p_cred_store;
    username_size = p_session->username_size;
    password_size = p_session->password_size;
    p_username    = p_session->p_username;
    p_password    = p_session->p_password;

    // Authenticate login against hash table
    p_item = ht_get(p_cred_store, p_username, username_size);

    if (NULL != p_item)
    {
        // NOTE: User already exists

        // Check if password doesn't match
        if (!(
            (p_item->value_size == password_size) &&
            (0 == memcmp(p_item->p_value, p_password, password_size))
        ))
        {
            // NOTE: Incorrect password

            if (p_server->b_verbose)
            {
                printf(
                    "Incorrect password for user: %.*s\n",
                    username_size,
                    p_username
                );
            }

            goto cleanup;
        }

        if (p_server->b_verbose)
        {
            printf(
                "Successful login to user: %.*s\n",
                username_size,
                p_username
            );
        }
    }
    else
    {
        // NOTE: User doesn't exist

        // Create new user
        ht_set(
            p_cred_store,
            p_username,
            username_size,
            p_password,
            password_size
        );

        if (p_server->b_verbose)
        {
            printf(
                "Created new user: %.*s\n",
                username_size,
                p_username
            );
        }
    }

    session_id = (p_appdata->next_session_id)++;

cleanup:
    return session_id;
}

status_t
user_logout (session_t * p_session, appdata_t * p_appdata)
{
    status_t status = STATUS_SUCCESS;

    if (
        (NULL == p_session) ||
        (NULL == p_session->p_username) ||
        (NULL == p_session->p_password) ||
        (NULL == p_appdata)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    memset(p_session->p_username, 0, g_username_size_max);
    memset(p_session->p_password, 0, g_password_size_max);

    p_session->username_size = 0u;
    p_session->password_size = 0u;
    p_session->session_id    = 0u;

cleanup:
    return status;
}

status_t
user_leave (session_t * p_session, appdata_t * p_appdata)
{
    status_t status = STATUS_SUCCESS;

    room_t * p_room = NULL;

    if (
        (NULL == p_session) ||
        (NULL == p_session->p_room) ||
        (NULL == p_appdata) ||
        (NULL == p_appdata->p_room_store)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_room = p_session->p_room;

    // Remove user session from room's sessions SLL
    sll_remove(p_room->p_sessions, &p_session, sizeof(p_session));

    p_session->p_room = NULL;

cleanup:
    return status;
}

bool
user_creds_len_valid (session_t * p_session, appdata_t * p_appdata)
{
    bool b_valid = true;
    UNUSED(p_appdata);

    uint16_t username_size = 0u;
    uint16_t password_size = 0u;

    if (
        (NULL == p_session) ||
        (NULL == p_session->p_username) ||
        (NULL == p_session->p_password)
    )
    {
        b_valid = false;
        goto cleanup;
    }

    username_size = p_session->username_size;
    password_size = p_session->password_size;

    // Validate username and password length
    if (!(
        (g_username_size_min <= username_size) &&
        (g_username_size_max >= username_size)
    ))
    {
        fprintf(
            stderr,
            "Username: 3-16 alphanumeric or underscore\n"
        );

        b_valid = false;
        goto cleanup;
    }

    if (!(
        (g_password_size_min <= password_size) &&
        (g_password_size_max >= password_size)
    ))
    {
        fprintf(
            stderr,
            "Password: 8-128 printable characters excluding space\n"
        );

        b_valid = false;
        goto cleanup;
    }

cleanup:
    return b_valid;
}

bool
user_creds_content_valid (session_t * p_session, appdata_t * p_appdata)
{
    bool b_valid = true;
    UNUSED(p_appdata);

    uint16_t   username_size = 0u;
    uint16_t   password_size = 0u;
    uint8_t  * p_username    = NULL;
    uint8_t  * p_password    = NULL;

    if (
        (NULL == p_session) ||
        (NULL == p_session->p_username) ||
        (NULL == p_session->p_password)
    )
    {
        b_valid = false;
        goto cleanup;
    }

    username_size = p_session->username_size;
    password_size = p_session->password_size;
    p_username    = p_session->p_username;
    p_password    = p_session->p_password;

    // Validate username and password content
    for (size_t idx = 0u; idx < username_size; idx++)
    {
        uint8_t chr = p_username[idx];
        if (!(isalnum(chr) || ('_' == chr)))
        {
            fprintf(
                stderr,
                "Username: 3-16 alphanumeric or underscore\n"
            );

            b_valid = false;
            goto cleanup;
        }
    }

    for (size_t idx = 0u; idx < password_size; idx++)
    {
        uint8_t chr = p_password[idx];
        if (!(isprint(chr) && (' ' != chr)))
        {
            fprintf(
                stderr,
                "Password: 8-128 printable characters excluding space\n"
            );

            b_valid = false;
            goto cleanup;
        }
    }

cleanup:
    return b_valid;
}

static appdata_t *
appdata_create (void)
{
    status_t status = STATUS_SUCCESS;

    appdata_t     * p_appdata       = NULL;
    ht_t          * p_cred_store    = NULL;
    sll_t         * p_room_store    = NULL;
    room_t        * p_room          = NULL;
    opcode_func_t * pp_opcode_funcs = NULL;

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

    status = ht_set(
        p_cred_store,
        "admin",
        5u,
        "password",
        8u
    );
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "ht_set failed in appdata_create\n");
        goto cleanup;
    }

    p_room_store = sll_create();
    if (NULL == p_room_store)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_appdata->p_room_store = p_room_store;

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
        fprintf(stderr, "sll_append failed in appdata_create\n");

        room_destroy(p_room);
        p_room = NULL;
        goto cleanup;
    }

    pp_opcode_funcs = calloc(UINT8_MAX + 1u, sizeof(*pp_opcode_funcs));
    if (NULL == pp_opcode_funcs)
    {
        fprintf(stderr, "calloc failed in appdata_create\n");
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
    pp_opcode_funcs[OPCODE_LIST]     = opcode_list;

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
