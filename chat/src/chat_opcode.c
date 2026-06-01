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
extern uint16_t const g_username_size_min;
extern uint16_t const g_username_size_max;
extern uint16_t const g_password_size_min;
extern uint16_t const g_password_size_max;

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
    uint8_t  * p_username,
    uint16_t   username_size,
    uint8_t  * p_password,
    uint16_t   password_size
);

/*!
 * @brief Send message to session
 *
 * @param[in] p_session Pointer to session
 * @param[in] p_msg     Pointer to message
 * @param[in] msg_size  Size of message in bytes
 *
 * @return Status of operation
 */
static status_t msg_send (
    session_t * p_session,
    uint8_t   * p_msg,
    uint16_t    msg_size
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

    if (
        (NULL == p_session) ||
        (NULL == p_request) ||
        (NULL == p_response) ||
        (NULL == p_session->p_server)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd   = p_session->sockfd;
    p_server = p_session->p_server;

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

    if (
        (NULL == p_session) ||
        (NULL == p_request) ||
        (NULL == p_response) ||
        (NULL == p_request->p_packet)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_request_packet = p_request->p_packet;

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

    if (
        (NULL == p_session) ||
        (NULL == p_request) ||
        (NULL == p_response) ||
        (NULL == p_request->p_packet) ||
        (NULL == p_response->p_packet)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_request_packet  = p_request->p_packet;
    p_response_packet = p_response->p_packet;

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

    if (
        (NULL == p_session) ||
        (NULL == p_request) ||
        (NULL == p_response) ||
        (NULL == p_session->p_server) ||
        (NULL == p_request->p_packet)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_server          = p_session->p_server;
    p_request_packet  = p_request->p_packet;

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
    int         sockfd            = -1;
    server_t  * p_server          = NULL;
    uint8_t   * p_request_packet  = NULL;
    uint8_t   * p_response_packet = NULL;
    uint32_t    session_id        = 0u;
    uint16_t    username_size     = 0u;
    uint16_t    password_size     = 0u;
    uint8_t   * p_username        = NULL;
    uint8_t   * p_password        = NULL;
    bool        b_locked          = false;

    if (
        (NULL == p_session) ||
        (NULL == p_request) ||
        (NULL == p_response) ||
        (NULL == p_session->p_server) ||
        (NULL == p_session->p_server->p_appdata) ||
        (NULL == p_request->p_packet) ||
        (NULL == p_response->p_packet)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_server          = p_session->p_server;
    p_request_packet  = p_request->p_packet;
    p_response_packet = p_response->p_packet;
    p_appdata         = p_server->p_appdata;

    user_leave(p_session, p_appdata);
    user_logout(p_session, p_appdata);

    p_response->opcode  = OPCODE_LOGIN;
    p_response->retcode = RETCODE_SUCCESS;

    login_hdr_t * p_hdr = (login_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    username_size = ntohs(p_hdr->username_size);
    password_size = ntohs(p_hdr->password_size);

    p_request->size += sizeof(*p_hdr);

    if ((p_request->size + username_size + password_size) > g_max_packet_size)
    {
        fprintf(stderr, "Login request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, username_size + password_size, g_chunk_size);
        p_response->retcode = RETCODE_OVERFLOW;
        goto cleanup;
    }

    if (!user_creds_len_valid(username_size, password_size))
    {
        sockutil_drain(sockfd, username_size + password_size, g_chunk_size);
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    p_username = p_session->p_username;
    p_password = p_session->p_password;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        username_size
    );

    memcpy(
        p_username,
        p_request_packet + p_request->size,
        username_size
    );
    p_request->size += username_size;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        password_size
    );

    memcpy(
        p_password,
        p_request_packet + p_request->size,
        password_size
    );
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
        goto cleanup;
    }

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    // Prevent integer overflow from sending session ID zero
    if (0u == p_appdata->next_session_id)
    {
        fprintf(stderr, "Why so many sessions?\n");
        p_appdata->next_session_id = 1u;
    }

    p_session->username_size = username_size;
    p_session->password_size = password_size;

    session_id = user_login(p_session, p_appdata);
    if (0u == session_id)
    {
        p_response->retcode = RETCODE_FAILURE;
    }

cleanup:
    if (
        (NULL != p_session) &&
        (NULL != p_response) &&
        (NULL != p_response->p_packet)
    )
    {
        // Copy session ID into response
        *(uint32_t *)(p_response_packet + p_response->size) = htonl(session_id);
        p_response->size      += FIELD_SIZE_SESSION_ID;
        p_session->session_id  = session_id;

        // Always set response size so packet sends entirely
        p_response->size = (
            FIELD_SIZE_OPCODE +
            FIELD_SIZE_RETCODE +
            FIELD_SIZE_SESSION_ID
        );
    }

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
    int         sockfd           = -1;
    server_t  * p_server         = NULL;
    uint8_t   * p_request_packet = NULL;
    bool        b_locked         = false;

    if (
        (NULL == p_session) ||
        (NULL == p_request) ||
        (NULL == p_response) ||
        (NULL == p_session->p_server) ||
        (NULL == p_session->p_server->p_appdata) ||
        (NULL == p_request->p_packet)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_server         = p_session->p_server;
    p_request_packet = p_request->p_packet;
    p_appdata        = p_server->p_appdata;

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

    user_leave(p_session, p_appdata);
    user_logout(p_session, p_appdata);

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

    appdata_t * p_appdata        = NULL;
    int         sockfd           = -1;
    server_t  * p_server         = NULL;
    uint8_t   * p_request_packet = NULL;
    room_t    * p_room           = NULL;
    uint8_t   * p_msg            = NULL;
    bool        b_locked         = false;

    if (
        (NULL == p_session) ||
        (NULL == p_request) ||
        (NULL == p_response) ||
        (NULL == p_session->p_server) ||
        (NULL == p_session->p_server->p_appdata) ||
        (NULL == p_request->p_packet)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_server         = p_session->p_server;
    p_request_packet = p_request->p_packet;
    p_appdata        = p_server->p_appdata;

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

    p_room = p_session->p_room;

    if (NULL == p_room)
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

    p_msg = p_request_packet + p_request->size;

    p_request->size += msg_size;

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

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
            (p_room != p_target->p_room)
        )
        {
            p_curr = p_curr->p_next;
            continue;
        }

        msg_send(p_target, p_msg, msg_size);

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
            p_room->name_size,
            p_room->p_name
        );
    }

cleanup:
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
    sll_t     * p_room_store      = NULL;
    node_t    * p_node            = NULL;
    int         sockfd            = -1;
    server_t  * p_server          = NULL;
    uint8_t   * p_request_packet  = NULL;
    room_t    * p_room            = NULL;
    uint8_t   * p_room_name       = NULL;
    uint16_t    room_name_size    = 0u;
    // uint8_t   * p_dm_name         = NULL;
    // uint16_t    dm_name_size      = 0u;
    // bool        b_private         = false;
    bool        b_locked          = false;

    if (
        (NULL == p_session) ||
        (NULL == p_request) ||
        (NULL == p_response) ||
        (NULL == p_session->p_server) ||
        (NULL == p_session->p_server->p_appdata) ||
        (NULL == p_request->p_packet)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_server         = p_session->p_server;
    p_request_packet = p_request->p_packet;
    p_appdata        = p_server->p_appdata;
    p_room_store     = p_appdata->p_room_store;

    if (NULL == p_room_store)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_JOIN;
    p_response->retcode = RETCODE_SUCCESS;

    join_hdr_t * p_hdr = (join_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    // b_private              = p_hdr->b_private;
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

    p_room_name      = p_request_packet + p_request->size;
    p_request->size += room_name_size;

    if (!ischartype_str((char *)p_room_name, room_name_size, isalnum))
    {
        p_response->retcode = RETCODE_FAILURE;
        fprintf(stderr, "Room name must be alphanumeric\n");
        goto cleanup;
    }

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    // TODO: Mutate room name to be canonical for private messaging
    // "user1+user2"
    // if (b_private)
    // {
    //     uint8_t * p_user1 = p_session->p_username;
    //     uint8_t * p_user2 = p_room_name;

    //     p_room_name    = p_dm_name;
    //     room_name_size = dm_name_size;
    // }

    user_leave(p_session, p_appdata);

    p_node = sll_get(p_room_store, p_room_name, room_name_size);
    if (NULL == p_node)
    {
        // NOTE: Room doesn't exist

        p_room = room_create((char *)p_room_name, room_name_size);
        if (NULL == p_room)
        {
            status = STATUS_ALLOC_FAILURE;
            fprintf(stderr, "room_create failed in opcode_join\n");
            p_response->retcode = RETCODE_FAILURE;
            goto cleanup;
        }

        // TODO: Set room as private if private flag is received
        // if (b_private)
        // {
        //     p_room->b_private = true;
        // }

        status = sll_append(p_room_store, &p_room, sizeof(p_room));
        if (STATUS_SUCCESS != status)
        {
            fprintf(stderr, "sll_append failed in opcode_join\n");
            p_response->retcode = RETCODE_FAILURE;
            room_destroy(p_room);
            p_room = NULL;
            goto cleanup;
        }

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

        p_node = sll_get(p_room_store, p_room_name, room_name_size);
        if (NULL == p_node)
        {
            fprintf(stderr, "sll_get failed in opcode_join\n");
            p_response->retcode = RETCODE_FAILURE;
            goto cleanup;
        }
    }

    p_room = *(room_t **)(p_node->p_data);

    // Check if current user session is allowed to enter private room
    // if (p_room->b_private)
    // {
    //     if (!((
    //         (p_session->username_size == p_room->user1_size) &&
    //         (0 == memcmp(
    //             p_session->p_username,
    //             p_room->p_user1,
    //             p_session->username_size
    //         ))
    //         ) ||
    //         ((p_session->username_size == p_room->user2_size) &&
    //         (0 == memcmp(
    //             p_session->p_username,
    //             p_room->p_user2,
    //             p_session->username_size
    //         ))
    //     )))
    //     {
    //         p_response->retcode = RETCODE_FAILURE;
    //         goto cleanup;
    //     }
    // }

    // Join room
    status = sll_append(p_room->p_sessions, &p_session, sizeof(p_session));
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "sll_append failed in opcode_join\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    p_session->p_room = p_room;

    if (p_server->b_verbose)
    {
        printf(
            "%.*s joined room: \"%.*s\"\n",
            p_session->username_size,
            p_session->p_username,
            p_room->name_size,
            p_room->p_name
        );
    }

cleanup:
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

    appdata_t * p_appdata        = NULL;
    sll_t     * p_room_store     = NULL;
    room_t    * p_room           = NULL;
    node_t    * p_curr           = NULL;
    session_t * p_target         = NULL;
    int         sockfd           = -1;
    server_t  * p_server         = NULL;
    uint8_t   * p_request_packet = NULL;
    uint8_t     flag             = 0u;
    bool        b_locked         = false;

    if (
        (NULL == p_session) ||
        (NULL == p_request) ||
        (NULL == p_response) ||
        (NULL == p_session->p_server) ||
        (NULL == p_session->p_server->p_appdata) ||
        (NULL == p_request->p_packet)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_server         = p_session->p_server;
    p_request_packet = p_request->p_packet;
    p_appdata        = p_server->p_appdata;
    p_room_store     = p_appdata->p_room_store;

    if (NULL == p_room_store)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_LIST;
    p_response->retcode = RETCODE_SUCCESS;

    list_hdr_t * p_hdr = (list_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    flag                   = p_hdr->flag;
    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    switch (flag)
    {
        case LIST_FLAG_ROOM:
            p_curr = p_room_store->p_head;
            while (NULL != p_curr)
            {
                p_room = *(room_t **)(p_curr->p_data);
                msg_send(p_session, p_room->p_name, p_room->name_size);
                p_curr = p_curr->p_next;
            }
            break;

        case LIST_FLAG_USER:
            if (NULL == p_session->p_room)
            {
                p_response->retcode = RETCODE_FAILURE;
                break;
            }
            p_curr = p_session->p_room->p_sessions->p_head;
            while (NULL != p_curr)
            {
                p_target = *(session_t **)(p_curr->p_data);
                msg_send(
                    p_session,
                    p_target->p_username,
                    p_target->username_size
                );
                p_curr = p_curr->p_next;
            }
            break;

        default:
            p_response->retcode = RETCODE_FAILURE;
            fprintf(stderr, "Unknown list flag: %02hhx\n", p_hdr->flag);
            break;
    }

cleanup:
    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
        b_locked = false;
    }
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

    if (
        (NULL == p_session) ||
        (NULL == p_request) ||
        (NULL == p_response) ||
        (NULL == p_session->p_server)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_server = p_session->p_server;

    if (
        (0u == p_session->session_id) ||
        (p_request->session_id != p_session->session_id)
    )
    {
        p_response->retcode = RETCODE_SESSION_ERROR;

        if (p_server->b_verbose)
        {
            fprintf(stderr, "Invalid session\n");
        }

        status = STATUS_INVALID_SESSION;
    }

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

static bool
user_creds_content_valid (
    uint8_t  * p_username,
    uint16_t   username_size,
    uint8_t  * p_password,
    uint16_t   password_size
)
{
    bool b_valid = true;

    if ((NULL == p_username) || (NULL == p_password))
    {
        b_valid = false;
        goto cleanup;
    }

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

static status_t
msg_send (
    session_t * p_session,
    uint8_t   * p_msg,
    uint16_t    msg_size
)
{
    status_t status = STATUS_SUCCESS;

    uint8_t            * p_packet = NULL;
    msg_recv_hdr_out_t   hdr      = {0};

    if ((NULL == p_session) || (NULL == p_msg))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_packet = calloc(sizeof(hdr) + msg_size, sizeof(*p_packet));
    if (NULL == p_packet)
    {
        fprintf(stderr, "calloc failed in msg_send\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    hdr.opcode   = OPCODE_MSG_RECV;
    hdr.retcode  = RETCODE_SUCCESS;
    hdr.msg_size = htons(msg_size);

    memcpy(p_packet, &hdr, sizeof(hdr));
    memcpy(p_packet + sizeof(hdr), p_msg, msg_size);

    pthread_mutex_lock(&(p_session->p_client->lock));
    sockutil_sendall(p_session->sockfd, p_packet, sizeof(hdr) + msg_size);
    pthread_mutex_unlock(&(p_session->p_client->lock));

    free(p_packet);
    p_packet = NULL;

cleanup:
    return status;
}

/*** end of file ***/
