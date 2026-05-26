/** @file chat_opcode.c
 *
 * @brief Chat opcode source
 *
 * @par
 *
 */

#include "chat_opcode.h"

extern uint32_t const g_max_packet_size;
extern uint32_t const g_chunk_size;

/*!
 * @brief Validate client session
 *
 * @param[in] p_session  Pointer to session
 * @param[in] p_request  Pointer to request
 * @param[in] p_response Pointer to response
 *
 * @return Status of operation
 */
static status_t validate_session(
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

/*!
 * @brief Affect session with logout
 *
 * @param[in] p_session Pointer to session
 *
 * @return Status of operation
 */
static status_t user_logout(session_t * p_session);

/*!
 * @brief Affect session with leave room
 *
 * @param[in] p_session    Pointer to session
 * @param[in] p_room_store Pointer to room storage
 *
 * @return Status of operation
 */
static status_t user_leave(session_t * p_session, ht_t * p_room_store);

/*!
 * @brief Check if credentials length is valid
 *
 * @param[in] username_size Size of username in bytes
 * @param[in] password_size Size of password in bytes
 *
 * @return Boolean if credentials length is valid
 */
static bool user_creds_len_valid(
    uint16_t username_size,
    uint16_t password_size
);

/*!
 * @brief Check if credentials content is valid
 *
 * @param[in] p_username    Pointer to username
 * @param[in] username_size Size of username in bytes
 * @param[in] p_password    Pointer to password
 * @param[in] password_size Size of password in bytes
 *
 * @return Boolean if credentials content is valid
 */
static bool user_creds_content_valid(
    char     * p_username,
    uint16_t   username_size,
    char     * p_password,
    uint16_t   password_size
);

status_t
opcode_default (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    int        sockfd   = -1;
    server_t * p_server = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd   = p_session->sockfd;
    p_server = p_session->p_server;

    if (NULL == p_server)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_DEFAULT;
    p_response->retcode = RETCODE_SUCCESS;

    if (p_server->b_verbose)
    {
        printf(
            "Unknown opcode 0x%02hhx on sockfd %d\n",
            p_request->opcode,
            sockfd
        );
    }

cleanup:
    return status;
}

status_t
opcode_ping (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    int       sockfd           = -1;
    uint8_t * p_request_packet = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_request_packet = p_request->p_packet;

    if (NULL == p_request_packet)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_PING;
    p_response->retcode = RETCODE_SUCCESS;

    ping_hdr_t * p_hdr = (ping_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

cleanup:
    return status;
}

status_t
opcode_echo (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    int       sockfd            = -1;
    uint8_t * p_request_packet  = NULL;
    uint8_t * p_response_packet = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_request_packet  = p_request->p_packet;
    p_response_packet = p_response->p_packet;

    if ((NULL == p_request_packet) || (NULL == p_response_packet))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_ECHO;
    p_response->retcode = RETCODE_SUCCESS;

    echo_hdr_t * p_hdr = (echo_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    uint16_t payload_size = ntohs(p_hdr->payload_size);
    p_request->session_id = ntohl(p_hdr->session_id);

    // Copy size field into response
    *(uint16_t *)(p_response_packet + p_response->size) = htons(
        payload_size
    );

    p_request->size += sizeof(*p_hdr);

    if ((p_request->size + payload_size) > g_max_packet_size)
    {
        // Zero out response size field
        p_response->retcode = RETCODE_OVERFLOW;
        fprintf(stderr, "Echo request size exceeds g_max_packet_size\n");
        memset(p_response_packet + p_response->size, 0, FIELD_SIZE_SIZE);
        sockutil_drain(sockfd, payload_size, g_chunk_size);
        payload_size = 0u;
    }

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        // Zero out response size field
        status = STATUS_SUCCESS;
        memset(p_response_packet + p_response->size, 0, FIELD_SIZE_SIZE);
        sockutil_drain(sockfd, payload_size, g_chunk_size);
        payload_size = 0u;
    }

    p_response->size += FIELD_SIZE_SIZE;

    // Receive payload
    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        payload_size
    );

    // Copy payload into response
    memcpy(
        p_response_packet + p_response->size,
        p_request_packet + p_request->size,
        payload_size
    );

    p_request->size  += payload_size;
    p_response->size += payload_size;

cleanup:
    return status;
}

status_t
opcode_quit (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    int        sockfd            = -1;
    server_t * p_server          = NULL;
    uint8_t  * p_request_packet  = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_server          = p_session->p_server;
    p_request_packet  = p_request->p_packet;

    if ((NULL == p_server) || (NULL == p_request_packet))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_QUIT;
    p_response->retcode = RETCODE_SUCCESS;

    quit_hdr_t * p_hdr = (quit_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    p_request->size += sizeof(*p_hdr);

    if (p_server->b_verbose)
    {
        printf("Quitting client session on sockfd %d\n", p_session->sockfd);
    }

cleanup:
    return status;
}

status_t
opcode_login (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t * p_appdata         = NULL;
    ht_t      * p_cred_store      = NULL;
    ht_t      * p_room_store      = NULL;
    item_t    * p_item            = NULL;
    int         sockfd            = -1;
    server_t  * p_server          = NULL;
    uint8_t   * p_request_packet  = NULL;
    uint8_t   * p_response_packet = NULL;
    uint32_t    session_id        = 0u;
    uint16_t    username_size     = 0u;
    uint16_t    password_size     = 0u;
    char      * p_username        = NULL;
    char      * p_password        = NULL;
    bool        b_locked          = false;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_server          = p_session->p_server;
    p_request_packet  = p_request->p_packet;
    p_response_packet = p_response->p_packet;

    if (
        (NULL == p_server) ||
        (NULL == p_request_packet) ||
        (NULL == p_response_packet)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_appdata = p_server->p_appdata;
    if (NULL == p_appdata)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_cred_store = p_appdata->p_cred_store;
    if (NULL == p_cred_store)
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

    p_response->opcode  = OPCODE_LOGIN;
    p_response->retcode = RETCODE_SUCCESS;

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    user_logout(p_session);
    user_leave(p_session, p_room_store);

    pthread_mutex_unlock(&(p_appdata->lock));
    b_locked = false;

    login_hdr_t * p_hdr = (login_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    username_size = ntohs(p_hdr->username_size);
    password_size = ntohs(p_hdr->password_size);
    session_id    = ntohl(p_hdr->session_id);

    p_request->size += sizeof(*p_hdr);

    if ((p_request->size + username_size + password_size) > g_max_packet_size)
    {
        fprintf(stderr, "Login request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, username_size + password_size, g_chunk_size);
        p_response->retcode = RETCODE_OVERFLOW;
        session_id = 0u;
        goto cpy_session_id;
    }

    if (!user_creds_len_valid(username_size, password_size))
    {
        sockutil_drain(sockfd, username_size + password_size, g_chunk_size);
        p_response->retcode = RETCODE_FAILURE;
        session_id = 0u;
        goto cpy_session_id;
    }

    p_username = calloc(1u, username_size);
    if (NULL == p_username)
    {
        fprintf(stderr, "calloc failed in opcode_login\n");
        sockutil_drain(sockfd, username_size + password_size, g_chunk_size);
        p_response->retcode = RETCODE_FAILURE;
        session_id = 0u;
        goto cpy_session_id;
    }

    p_password = calloc(1u, password_size);
    if (NULL == p_password)
    {
        fprintf(stderr, "calloc failed in opcode_login\n");
        sockutil_drain(sockfd, username_size + password_size, g_chunk_size);
        p_response->retcode = RETCODE_FAILURE;
        session_id = 0u;
        goto cpy_session_id;
    }

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        username_size
    );

    memcpy(p_username, p_request_packet + p_request->size, username_size);
    p_request->size += username_size;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        password_size
    );

    memcpy(p_password, p_request_packet + p_request->size, password_size);
    p_request->size += password_size;

    if (p_server->b_verbose)
    {
        printf(
            "Attempting login with username=\"%.*s\", password=\"%.*s\"\n",
            username_size,
            p_username,
            password_size,
            p_password
        );
    }

    if (!user_creds_content_valid(
        p_username,
        username_size,
        p_password,
        password_size
    ))
    {
        p_response->retcode = RETCODE_FAILURE;
        session_id = 0u;
        goto cpy_session_id;
    }

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    // Prevent integer overflow from sending session ID zero
    if (0u == p_appdata->next_session_id)
    {
        fprintf(stderr, "Why so many sessions?\n");
        p_appdata->next_session_id = 1u;
    }

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

            p_response->retcode = RETCODE_FAILURE;

            session_id = 0u;

            if (p_server->b_verbose)
            {
                printf(
                    "Incorrect password for user: %.*s\n",
                    username_size,
                    p_username
                );
            }

            goto cpy_session_id;
        }

        if (p_server->b_verbose)
        {
            printf(
                "Successful login to user: %.*s\n",
                p_session->username_size,
                p_session->p_username
            );
        }
    }
    else
    {
        // NOTE: User doesn't exist

        // Create new user
        status = ht_set(
            p_cred_store,
            p_username,
            username_size,
            p_password,
            password_size
        );

        if (STATUS_SUCCESS != status)
        {
            fprintf(stderr, "ht_set failed\n");
            status = STATUS_SUCCESS;

            p_response->retcode = RETCODE_FAILURE;
            session_id = 0u;
            goto cpy_session_id;
        }

        if (p_server->b_verbose)
        {
            printf(
                "Created new user: %.*s\n",
                p_session->username_size,
                p_session->p_username
            );
        }
    }

    p_response->retcode   = RETCODE_SUCCESS;
    session_id            = (p_appdata->next_session_id)++;
    p_session->session_id = session_id;

    free(p_session->p_username);
    p_session->p_username    = p_username;
    p_session->username_size = username_size;
    p_username               = NULL;

    free(p_session->p_password);
    p_session->p_password    = p_password;
    p_session->password_size = password_size;
    p_password               = NULL;

cpy_session_id:
    // Copy session ID into response
    *(uint32_t *)(p_response_packet + p_response->size) = htonl(
        session_id
    );

    p_response->size += FIELD_SIZE_SESSION_ID;

cleanup:
    free(p_username);
    p_username = NULL;

    free(p_password);
    p_password = NULL;

    // Always set response size so packet sends entirely
    p_response->size = (
        FIELD_SIZE_OPCODE +
        FIELD_SIZE_RETCODE +
        FIELD_SIZE_SESSION_ID
    );

    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
        b_locked = false;
    }
    return status;
}

status_t
opcode_logout (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t * p_appdata        = NULL;
    ht_t      * p_room_store     = NULL;
    int         sockfd           = -1;
    server_t  * p_server         = NULL;
    uint8_t   * p_request_packet = NULL;
    bool        b_locked         = false;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_server          = p_session->p_server;
    p_request_packet  = p_request->p_packet;

    if ((NULL == p_server) || (NULL == p_request_packet))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

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

    p_response->opcode  = OPCODE_LOGOUT;
    p_response->retcode = RETCODE_SUCCESS;

    logout_hdr_t * p_hdr = (logout_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    p_response->retcode = RETCODE_SUCCESS;

    if (p_server->b_verbose)
    {
        printf(
            "Successful logout from user: %.*s\n",
            p_session->username_size,
            p_session->p_username
        );
    }

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    user_logout(p_session);
    user_leave(p_session, p_room_store);

cleanup:
    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
        b_locked = false;
    }
    return status;
}

status_t
opcode_msg_send (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t * p_appdata         = NULL;
    ht_t      * p_room_store      = NULL;
    item_t    * p_item            = NULL;
    node_t    * p_node            = NULL;
    int         sockfd            = -1;
    server_t  * p_server          = NULL;
    uint8_t   * p_request_packet  = NULL;
    room_t    * p_room            = NULL;
    char      * p_msg         = NULL;
    uint8_t   * p_msg_packet  = NULL;
    bool        b_locked          = false;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_server          = p_session->p_server;
    p_request_packet  = p_request->p_packet;

    if ((NULL == p_server) || (NULL == p_request_packet))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

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

    p_response->opcode  = OPCODE_MSG_SEND;
    p_response->retcode = RETCODE_SUCCESS;

    msg_send_hdr_t * p_hdr = (msg_send_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    uint16_t msg_size      = ntohs(p_hdr->msg_size);
    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    if ((p_request->size + msg_size) > g_max_packet_size)
    {
        p_response->retcode = RETCODE_OVERFLOW;
        fprintf(stderr, "msg send request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, msg_size, g_chunk_size);
        goto cleanup;
    }

    // Receive msg
    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        msg_size
    );

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    if (NULL == p_session->p_room_name)
    {
        p_response->retcode = RETCODE_FAILURE;

        if (p_server->b_verbose)
        {
            printf(
                "%.*s is not in a room\n",
                p_session->username_size,
                p_session->p_username
            );
        }

        goto cleanup;
    }

    p_msg = calloc(1u, msg_size);
    if (NULL == p_msg)
    {
        p_response->retcode = RETCODE_FAILURE;
        fprintf(stderr, "calloc failed in opcode_msg_send\n");
        goto cleanup;
    }

    memcpy(p_msg, p_request_packet + p_request->size, msg_size);

    p_request->size += msg_size;

    uint32_t msg_packet_size = sizeof(msg_recv_hdr_out_t) + msg_size;

    p_msg_packet = calloc(1u, msg_packet_size);
    if (NULL == p_msg_packet)
    {
        p_response->retcode = RETCODE_FAILURE;
        fprintf(stderr, "calloc failed in opcode_msg_send\n");
        goto cleanup;
    }

    msg_recv_hdr_out_t * p_msg_hdr = (msg_recv_hdr_out_t *)p_msg_packet;

    p_msg_hdr->opcode   = OPCODE_MSG_RECV;
    p_msg_hdr->retcode  = RETCODE_SUCCESS;
    p_msg_hdr->msg_size = htons(msg_size);

    memcpy(p_msg_packet + sizeof(*p_msg_hdr), p_msg, msg_size);

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    p_item = ht_get(
        p_room_store,
        p_session->p_room_name,
        p_session->room_name_size
    );
    if (NULL == p_item)
    {
        // NOTE: Room doesn't exist

        printf(
            "Room %.*s doesn't exist\n",
            p_session->room_name_size,
            p_session->p_room_name
        );

        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    p_room = p_item->p_value;

    p_node = sll_get(p_room->p_sessions, &p_session, sizeof(p_session));
    if (NULL == p_node)
    {
        // NOTE: Session is not member of room

        printf(
            "Session of user %.*s is not member of room %.*s\n",
            p_session->username_size,
            p_session->p_username,
            p_session->room_name_size,
            p_session->p_room_name
        );

        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    // Send msg to all sessions in room
    node_t * p_curr = p_room->p_sessions->p_head;
    while (NULL != p_curr)
    {
        // Send msg to single session
        session_t * p_target = *(session_t **)(p_curr->p_data);
        if (
            (0u == p_target->session_id) ||
            (0u == p_target->username_size) ||
            (NULL == p_target->p_username) ||
            (p_room->name_size != p_target->room_name_size) ||
            (NULL == p_target->p_room_name) ||
            (0 != memcmp(
                p_target->p_room_name,
                p_room->p_name,
                p_room->name_size
            ))
        )
        {
            p_curr = p_curr->p_next;
            continue;
        }

        pthread_mutex_lock(&(p_target->p_client->lock));
        sockutil_sendall(
            p_target->sockfd,
            p_msg_packet,
            msg_packet_size
        );
        pthread_mutex_unlock(&(p_target->p_client->lock));

        p_curr = p_curr->p_next;
    }

    if (p_server->b_verbose)
    {
        printf(
            "%.*s sent msg \"%.*s\" to room: \"%.*s\"\n",
            p_session->username_size,
            p_session->p_username,
            msg_size,
            p_msg,
            p_session->room_name_size,
            p_session->p_room_name
        );
    }

cleanup:
    free(p_msg_packet);
    p_msg_packet = NULL;

    free(p_msg);
    p_msg = NULL;

    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
        b_locked = false;
    }
    return status;
}

status_t
opcode_join (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t * p_appdata         = NULL;
    ht_t      * p_room_store      = NULL;
    item_t    * p_item            = NULL;
    int         sockfd            = -1;
    server_t  * p_server          = NULL;
    uint8_t   * p_request_packet  = NULL;
    room_t    * p_room            = NULL;
    char      * p_room_name       = NULL;
    uint16_t    room_name_size    = 0u;
    bool        b_locked          = false;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_server          = p_session->p_server;
    p_request_packet  = p_request->p_packet;

    if ((NULL == p_server) || (NULL == p_request_packet))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_JOIN;
    p_response->retcode = RETCODE_SUCCESS;

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

    join_hdr_t * p_hdr = (join_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    room_name_size         = ntohs(p_hdr->room_name_size);
    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    if ((p_request->size + room_name_size) > g_max_packet_size)
    {
        p_response->retcode = RETCODE_OVERFLOW;
        fprintf(stderr, "Join request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, room_name_size, g_chunk_size);
        goto cleanup;
    }

    // Receive room name
    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        room_name_size
    );

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    p_room_name = calloc(1u, room_name_size);
    if (NULL == p_room_name)
    {
        p_response->retcode = RETCODE_FAILURE;
        fprintf(stderr, "calloc failed in opcode_join\n");
        goto cleanup;
    }

    memcpy(p_room_name, p_request_packet + p_request->size, room_name_size);

    p_request->size += room_name_size;

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    user_leave(p_session, p_room_store);

    p_item = ht_get(p_room_store, p_room_name, room_name_size);
    if (NULL == p_item)
    {
        // NOTE: Room doesn't exist

        p_room = room_create(p_room_name, room_name_size);
        if (NULL == p_room)
        {
            status = STATUS_ALLOC_FAILURE;
            fprintf(stderr, "room_create failed\n");
            p_response->retcode = RETCODE_FAILURE;
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
            p_response->retcode = RETCODE_FAILURE;

            room_destroy(p_room);
            p_room = NULL;
            goto cleanup;
        }

        // Free room after hash table makes its own copy
        free(p_room);
        p_room = NULL;

        if (p_server->b_verbose)
        {
            printf(
                "%.*s created new room: \"%.*s\"\n",
                p_session->username_size,
                p_session->p_username,
                room_name_size,
                p_room_name
            );
        }

        p_item = ht_get(p_room_store, p_room_name, room_name_size);
        if (NULL == p_item)
        {
            fprintf(stderr, "ht_get failed\n");
            p_response->retcode = RETCODE_FAILURE;
            goto cleanup;
        }
    }

    p_room = p_item->p_value;

    // Join room
    status = sll_append(p_room->p_sessions, &p_session, sizeof(p_session));
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "sll_append failed\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    free(p_session->p_room_name);
    p_session->p_room_name    = p_room_name;
    p_session->room_name_size = room_name_size;
    p_room_name = NULL;

    if (p_server->b_verbose)
    {
        printf(
            "%.*s joined room: \"%.*s\"\n",
            p_session->username_size,
            p_session->p_username,
            p_session->room_name_size,
            p_session->p_room_name
        );
    }

cleanup:
    free(p_room_name);
    p_room_name = NULL;

    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
        b_locked = false;
    }
    return status;
}

status_t
opcode_list (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    int       sockfd           = -1;
    uint8_t * p_request_packet = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_request_packet = p_request->p_packet;

    if (NULL == p_request_packet)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_LIST;
    p_response->retcode = RETCODE_SUCCESS;

    list_hdr_t * p_hdr = (list_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

cleanup:
    return status;
}

static status_t
validate_session (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    server_t * p_server = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_server = p_session->p_server;
    if (NULL == p_server)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (
        (0u == p_session->session_id) ||
        (p_request->session_id != p_session->session_id)
    )
    {
        p_response->retcode = RETCODE_SESSION_ERROR;

        if (p_session->p_server->b_verbose)
        {
            fprintf(stderr, "Invalid session\n");
        }

        status = STATUS_INVALID_SESSION;
    }

cleanup:
    return status;
}

static status_t
user_logout (session_t * p_session)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_session)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    free(p_session->p_username);
    p_session->p_username = NULL;

    free(p_session->p_password);
    p_session->p_password = NULL;

    p_session->username_size = 0u;
    p_session->password_size = 0u;
    p_session->session_id    = 0u;

cleanup:
    return status;
}

static status_t
user_leave (session_t * p_session, ht_t * p_room_store)
{
    status_t status = STATUS_SUCCESS;

    item_t * p_item = NULL;
    room_t * p_room = NULL;

    if ((NULL == p_session) || (NULL == p_room_store))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

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

    p_session->room_name_size = 0u;

cleanup:
    return status;
}

static bool
user_creds_len_valid (
    uint16_t username_size,
    uint16_t password_size
)
{
    bool b_valid = true;

    // Validate username and password length
    if (!((3u <= username_size) && (16u >= username_size)))
    {
        fprintf(
            stderr,
            "Username: 3-16 alphanumeric or underscore\n"
        );

        b_valid = false;
        goto cleanup;
    }

    if (!((8u <= password_size) && (128u >= password_size)))
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

static bool
user_creds_content_valid (
    char     * p_username,
    uint16_t   username_size,
    char     * p_password,
    uint16_t   password_size
)
{
    bool b_valid = true;

    if ((NULL == p_username) || (NULL == p_password))
    {
        goto cleanup;
    }

    // Validate username and password content
    for (size_t idx = 0u; idx < username_size; idx++)
    {
        char chr = p_username[idx];
        if (!(isalnum((unsigned char)chr) || ('_' == chr)))
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
        char chr = p_password[idx];
        if (!(isprint((unsigned char)chr) && (' ' != chr)))
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

/*** end of file ***/
