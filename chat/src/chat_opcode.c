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
 * @brief Send message to session
 *
 * @param[in] p_session Pointer to session
 * @param[in] flag      Flag for type of response
 * @param[in] p_msg     Pointer to message
 * @param[in] msg_size  Size of message in bytes
 *
 * @return Status of operation
 */
static status_t msg_send(
    session_t     * p_session,
    uint8_t         flag,
    uint8_t const * p_msg,
    uint16_t        msg_size
);

/*!
 * @brief Send message to all sessions in room that have username
 *
 * @param[in] p_room        Pointer to room
 * @param[in] p_username    Pointer to username
 * @param[in] username_size Size of username in bytes
 * @param[in] flag          Flag for type of response
 * @param[in] p_msg         Pointer to message
 * @param[in] msg_size      Size of message in bytes
 *
 * @return Status of operation
 */
static status_t msg_send_username(
    room_t        * p_room,
    uint8_t       * p_username,
    uint16_t        username_size,
    uint8_t         flag,
    uint8_t const * p_msg,
    uint16_t        msg_size
);

/*!
 * @brief Send message to all sessions in room
 *
 * @param[in] p_room   Pointer to room
 * @param[in] flag     Flag for type of response
 * @param[in] p_msg    Pointer to message
 * @param[in] msg_size Size of message in bytes
 *
 * @return Status of operation
 */
static status_t msg_send_room(
    room_t        * p_room,
    uint8_t         flag,
    uint8_t const * p_msg,
    uint16_t        msg_size
);

/*!
 * @brief Check if session is an admin
 *
 * @param[in] p_session Pointer to session
 * @param[in] p_appdata Pointer to application data
 *
 * @return Status of operation
 */
static bool
is_admin(session_t * p_session, appdata_t * p_appdata);

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

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    user_logout(p_session, p_appdata);

    pthread_mutex_unlock(&(p_appdata->lock));
    b_locked = false;

    p_response->opcode  = OPCODE_LOGIN;
    p_response->retcode = RETCODE_SUCCESS;

    login_hdr_t * p_hdr = (login_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    username_size = ntohs(p_hdr->username_size);
    password_size = ntohs(p_hdr->password_size);

    p_session->username_size = username_size;
    p_session->password_size = password_size;

    p_request->size += sizeof(*p_hdr);

    if ((p_request->size + username_size + password_size) > g_max_packet_size)
    {
        fprintf(stderr, "Login request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, username_size + password_size, g_chunk_size);
        p_response->retcode = RETCODE_OVERFLOW;
        goto cleanup;
    }

    if (!user_creds_len_valid(p_session, NULL))
    {
        sockutil_drain(sockfd, username_size + password_size, g_chunk_size);
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    p_username       = p_request_packet + p_request->size;
    p_request->size += username_size;
    p_password       = p_request_packet + p_request->size;
    p_request->size += password_size;

    sockutil_recvall(sockfd, p_username, username_size);
    memcpy(p_session->p_username, p_username, username_size);
    sockutil_recvall(sockfd, p_password, password_size);
    memcpy(p_session->p_password, p_password, password_size);

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

    if (!user_creds_content_valid(p_session, NULL))
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
    uint16_t    msg_size         = 0u;
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

    msg_size               = ntohs(p_hdr->msg_size);
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
    msg_send_room(p_room, MSG_FLAG_MSG, p_msg, msg_size);

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

    user_leave(p_session, p_appdata);

    p_session->p_room = p_room;

    status = user_join(p_session, p_appdata);
    if (STATUS_SUCCESS != status)
    {
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    if (p_server->b_verbose)
    {
        if (NULL == p_session->p_room)
        {
            printf(
                "%.*s is already in room: \"%.*s\"\n",
                p_session->username_size,
                p_session->p_username,
                p_room->name_size,
                p_room->p_name
            );
            p_response->retcode = RETCODE_DUPLICATE;
        }
        else
        {
            printf(
                "%.*s joined room: \"%.*s\"\n",
                p_session->username_size,
                p_session->p_username,
                p_room->name_size,
                p_room->p_name
            );
        }
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
                msg_send(
                    p_session,
                    MSG_FLAG_LIST,
                    p_room->p_name,
                    p_room->name_size
                );
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
                    MSG_FLAG_LIST,
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

status_t
opcode_request (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t * p_appdata        = NULL;
    sll_t     * p_room_store     = NULL;
    room_t    * p_room           = NULL;
    item_t    * p_item           = NULL;
    int         sockfd           = -1;
    server_t  * p_server         = NULL;
    uint8_t   * p_request_packet = NULL;
    ht_t      * p_pm_reqs        = NULL;
    ht_t      * p_file_reqs      = NULL;
    uint8_t     flag_type        = 0u;
    uint16_t    username_size    = 0u;
    uint8_t   * p_username       = NULL;
    uint8_t   * p_msg            = NULL;
    int         written          = -1;
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

    p_response->opcode  = OPCODE_REQUEST;
    p_response->retcode = RETCODE_SUCCESS;

    req_hdr_t * p_hdr = (req_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    flag_type              = p_hdr->flag_type;
    username_size          = ntohs(p_hdr->username_size);
    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    if ((p_request->size + username_size) > g_max_packet_size)
    {
        fprintf(stderr, "Request request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, username_size, g_chunk_size);
        p_response->retcode = RETCODE_OVERFLOW;
        goto cleanup;
    }

    p_username       = p_request_packet + p_request->size;
    p_request->size += username_size;

    sockutil_recvall(sockfd, p_username, username_size);

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    p_room = p_session->p_room;
    if (NULL == p_room)
    {
        fprintf(stderr, "Sending user not in room\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    p_pm_reqs   = p_room->p_pm_reqs;
    p_file_reqs = p_room->p_file_reqs;

    ht_display(p_pm_reqs, ", ");

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    if (NULL == session_by_username(p_room, p_username, username_size))
    {
        fprintf(stderr, "Receiving user not in room\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    p_msg = calloc(g_max_packet_size, sizeof(*p_msg));
    if (NULL == p_msg)
    {
        fprintf(stderr, "calloc failed in opcode_request\n");
        p_response->retcode = RETCODE_FAILURE;
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    switch (flag_type)
    {
        case REQ_FLAG_TYPE_PM:
            p_item = ht_get(p_pm_reqs, p_username, username_size);
            if (0u != p_item->value_size)
            {
                p_response->retcode = RETCODE_PENDING;
                goto cleanup;
            }

            ht_set(
                p_pm_reqs,
                p_username,
                username_size,
                p_session->p_username,
                p_session->username_size
            );

            written = snprintf(
                (char *)p_msg,
                g_max_packet_size,
                "PM request recieved from %.*s",
                p_session->username_size,
                p_session->p_username
            );

            // Send notification
            msg_send_username(
                p_room,
                p_username,
                username_size,
                MSG_FLAG_NOTIF,
                p_msg,
                written
            );

            break;

        case REQ_FLAG_TYPE_FILE:
            p_item = ht_get(p_file_reqs, p_username, username_size);
            if (0u != p_item->value_size)
            {
                p_response->retcode = RETCODE_PENDING;
                goto cleanup;
            }

            ht_set(
                p_file_reqs,
                p_username,
                username_size,
                p_session->p_username,
                p_session->username_size
            );

            written = snprintf(
                (char *)p_msg,
                g_max_packet_size,
                "File transfer request recieved from %.*s",
                p_session->username_size,
                p_session->p_username
            );

            // Send notification
            msg_send_username(
                p_room,
                p_username,
                username_size,
                MSG_FLAG_NOTIF,
                p_msg,
                written
            );

            break;

        default:
            p_response->retcode = RETCODE_FAILURE;
            fprintf(
                stderr,
                "Unknown request flag type: %02hhx\n",
                p_hdr->flag_type
            );
            break;
    }

cleanup:
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
opcode_respond (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t * p_appdata        = NULL;
    sll_t     * p_room_store     = NULL;
    room_t    * p_room           = NULL;
    room_t    * p_new_room       = NULL;
    item_t    * p_item           = NULL;
    int         sockfd           = -1;
    server_t  * p_server         = NULL;
    uint8_t   * p_request_packet = NULL;
    ht_t      * p_pm_reqs        = NULL;
    ht_t      * p_file_reqs      = NULL;
    uint8_t     flag_type        = 0u;
    uint8_t     flag_choice      = 0u;
    uint16_t    username_size    = 0u;
    uint8_t   * p_username       = NULL;
    uint8_t   * p_msg            = NULL;
    node_t    * p_curr           = NULL;
    session_t * p_target         = NULL;
    uint8_t   * p_room_name      = NULL;
    uint8_t   * p_user1          = NULL;
    uint16_t    user1_size       = 0u;
    uint8_t   * p_user2          = NULL;
    uint16_t    user2_size       = 0u;
    uint16_t    room_name_size   = 0u;
    int         written          = -1;
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

    p_response->opcode  = OPCODE_RESPOND;
    p_response->retcode = RETCODE_SUCCESS;

    resp_hdr_t * p_hdr = (resp_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    flag_type              = p_hdr->flag_type;
    flag_choice            = p_hdr->flag_choice;
    username_size          = ntohs(p_hdr->username_size);
    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    if ((p_request->size + username_size) > g_max_packet_size)
    {
        fprintf(stderr, "Respond request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, username_size, g_chunk_size);
        p_response->retcode = RETCODE_OVERFLOW;
        goto cleanup;
    }

    p_username       = p_request_packet + p_request->size;
    p_request->size += username_size;

    sockutil_recvall(sockfd, p_username, username_size);

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    p_room = p_session->p_room;
    if (NULL == p_room)
    {
        fprintf(stderr, "Sending user not in room\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    p_pm_reqs   = p_room->p_pm_reqs;
    p_file_reqs = p_room->p_file_reqs;

    ht_display(p_pm_reqs, ", ");

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    if (NULL == session_by_username(p_room, p_username, username_size))
    {
        fprintf(stderr, "Receiving user not in room\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    p_msg = calloc(g_max_packet_size, sizeof(*p_msg));
    if (NULL == p_msg)
    {
        fprintf(stderr, "calloc failed in opcode_respond\n");
        p_response->retcode = RETCODE_FAILURE;
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    switch (flag_type)
    {
        case RESP_FLAG_TYPE_PM:
            // Get request sent to self
            p_item = ht_get(
                p_pm_reqs,
                p_session->p_username,
                p_session->username_size
            );
            if (0u == p_item->value_size)
            {
                fprintf(stderr, "No pending request from anyone\n");
                p_response->retcode = RETCODE_NOT_PENDING;
                goto cleanup;
            }

            // Verify sender of request is the username being responded to
            if (
                (p_item->value_size != username_size) ||
                (0 != memcmp(p_item->p_value, p_username, username_size))
            )
            {
                fprintf(
                    stderr,
                    "No pending request from %.*s\n",
                    username_size,
                    p_username
                );
                p_response->retcode = RETCODE_NOT_PENDING;
                goto cleanup;
            }

            // Send notification if declined
            if (RESP_FLAG_CHOICE_DECLINE == flag_choice)
            {
                msg_send(
                    p_session,
                    MSG_FLAG_NOTIF,
                    (uint8_t *)"Responded: decline",
                    18u
                );

                written = snprintf(
                    (char *)p_msg,
                    g_max_packet_size,
                    "PM request declined by %.*s",
                    p_session->username_size,
                    p_session->p_username
                );

                // Send notification
                msg_send_username(
                    p_room,
                    p_username,
                    username_size,
                    MSG_FLAG_NOTIF,
                    p_msg,
                    written
                );
            }
            else
            {
                p_room_name = calloc(
                    2u * g_username_size_max + 1u,
                    sizeof(*p_room_name)
                );
                if (NULL == p_room_name)
                {
                    fprintf(stderr, "calloc failed in opcode_respond\n");
                    status = STATUS_ALLOC_FAILURE;
                    goto cleanup;
                }

                if (0 < memcmp(
                    p_session->p_username,
                    p_username,
                    (
                        p_session->username_size < username_size ?
                        p_session->username_size :
                        username_size
                    )
                ))
                {
                    // NOTE: p_session->p_username is greater than p_username
                    p_user1    = p_username;
                    user1_size = username_size;
                    p_user2    = p_session->p_username;
                    user2_size = p_session->username_size;
                }
                else
                {
                    // NOTE: p_session->p_username is less than p_username
                    p_user1    = p_session->p_username;
                    user1_size = p_session->username_size;
                    p_user2    = p_username;
                    user2_size = username_size;
                }

                memcpy(p_room_name, p_user1, user1_size);
                p_room_name[user1_size] = '+';
                memcpy(p_room_name + user1_size + 1u, p_user2, user2_size);

                room_name_size = user1_size + 1u + user2_size;

                // Create room and make both users join
                p_new_room = room_create(
                    (char *)p_room_name,
                    room_name_size
                );
                if (NULL == p_new_room)
                {
                    status = STATUS_ALLOC_FAILURE;
                    goto cleanup;
                }

                status = sll_append(
                    p_room_store,
                    &p_new_room,
                    sizeof(p_new_room)
                );
                if (STATUS_SUCCESS != status)
                {
                    fprintf(stderr, "sll_append failed in opcode_respond\n");
                    room_destroy(p_new_room);
                    p_new_room = NULL;
                    goto cleanup;
                }

                user_leave(p_session, p_appdata);
                p_session->p_room = p_new_room;
                user_join(p_session, p_appdata);
                msg_send(
                    p_session,
                    MSG_FLAG_JOIN,
                    p_room_name,
                    room_name_size
                );

                p_curr = p_room->p_sessions->p_head;
                while (NULL != p_curr)
                {
                    p_target = *(session_t **)(p_curr->p_data);
                    if (
                        (p_target->username_size == username_size) &&
                        (0 == memcmp(
                            p_target->p_username,
                            p_username,
                            username_size
                        ))
                    )
                    {
                        user_leave(p_target, p_appdata);
                        p_target->p_room = p_new_room;
                        user_join(p_target, p_appdata);
                        msg_send(
                            p_target,
                            MSG_FLAG_JOIN,
                            p_room_name,
                            room_name_size
                        );
                        break;
                    }

                    p_curr = p_curr->p_next;
                }
            }

            // Reset request item for self
            ht_set(
                p_room->p_pm_reqs,
                p_session->p_username,
                p_session->username_size,
                "",
                0u
            );

            break;

        case RESP_FLAG_TYPE_FILE:
            // Get request sent to self
            p_item = ht_get(
                p_file_reqs,
                p_session->p_username,
                p_session->username_size
            );
            if (0u == p_item->value_size)
            {
                fprintf(stderr, "No pending request from anyone\n");
                p_response->retcode = RETCODE_NOT_PENDING;
                goto cleanup;
            }

            // Verify sender of request is the username being responded to
            if (
                (p_item->value_size != username_size) ||
                (0 != memcmp(p_item->p_value, p_username, username_size))
            )
            {
                fprintf(
                    stderr,
                    "No pending request from %.*s\n",
                    username_size,
                    p_username
                );
                p_response->retcode = RETCODE_NOT_PENDING;
                goto cleanup;
            }

            // Send notification if declined
            if (RESP_FLAG_CHOICE_DECLINE == flag_choice)
            {
                msg_send(
                    p_session,
                    MSG_FLAG_NOTIF,
                    (uint8_t *)"Responded: decline",
                    18u
                );

                written = snprintf(
                    (char *)p_msg,
                    g_max_packet_size,
                    "File transfer request declined by %.*s",
                    p_session->username_size,
                    p_session->p_username
                );

                // Send notification
                msg_send_username(
                    p_room,
                    p_username,
                    username_size,
                    MSG_FLAG_NOTIF,
                    p_msg,
                    written
                );
            }
            else
            {
                // Allow file to be transferred later
                memcpy(p_session->p_user_allow, p_username, username_size);
                p_session->user_allow_size = username_size;
            }

            // Reset request item for self
            ht_set(
                p_room->p_file_reqs,
                p_session->p_username,
                p_session->username_size,
                "",
                0u
            );

            break;

        default:
            p_response->retcode = RETCODE_FAILURE;
            fprintf(
                stderr,
                "Unknown respond flag type: %02hhx\n",
                p_hdr->flag_type
            );
            break;
    }

cleanup:
    free(p_room_name);
    p_room_name = NULL;

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
opcode_file_send (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t       * p_appdata        = NULL;
    sll_t           * p_room_store     = NULL;
    room_t          * p_room           = NULL;
    int               sockfd           = -1;
    server_t        * p_server         = NULL;
    uint8_t         * p_request_packet = NULL;
    uint16_t          username_size    = 0u;
    uint16_t          filename_size    = 0u;
    uint16_t          file_size        = 0u;
    uint8_t         * p_username       = NULL;
    uint8_t         * p_filename       = NULL;
    uint8_t         * p_file           = NULL;
    uint16_t          msg_size         = 0u;
    uint8_t         * p_msg            = NULL;
    session_t       * p_target         = NULL;
    bool              b_locked         = false;
    file_send_hdr_t * p_hdr            = NULL;
    file_recv_hdr_t * p_recv_hdr       = NULL;

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

    p_response->opcode  = OPCODE_FILE_SEND;
    p_response->retcode = RETCODE_SUCCESS;

    p_hdr = (file_send_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    username_size          = ntohs(p_hdr->username_size);
    filename_size          = ntohs(p_hdr->filename_size);
    file_size              = ntohs(p_hdr->file_size);
    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    if (
        (
            p_request->size +
            username_size +
            filename_size +
            file_size
        ) > g_max_packet_size
    )
    {
        fprintf(stderr, "File send request size exceeds g_max_packet_size\n");
        sockutil_drain(
            sockfd,
            (
                username_size +
                filename_size +
                file_size
            ),
            g_chunk_size
        );
        p_response->retcode = RETCODE_OVERFLOW;
        goto cleanup;
    }

    p_username       = p_request_packet + p_request->size;
    p_request->size += username_size;

    p_filename       = p_request_packet + p_request->size;
    p_request->size += filename_size;

    p_file           = p_request_packet + p_request->size;
    p_request->size += file_size;

    sockutil_recvall(sockfd, p_username, username_size);
    sockutil_recvall(sockfd, p_filename, filename_size);
    sockutil_recvall(sockfd, p_file, file_size);

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    p_room = p_session->p_room;
    if (NULL == p_room)
    {
        fprintf(stderr, "Sending user not in room\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    p_target = session_by_username(p_room, p_username, username_size);

    if (NULL == p_target)
    {
        fprintf(stderr, "Receiving user not in room\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    // Reject unallowed file sends from current session
    if (
        (p_target->user_allow_size != p_session->username_size) ||
        (0 != memcmp(
            p_target->p_user_allow,
            p_session->p_username,
            p_session->username_size
        ))
    )
    {
        fprintf(stderr, "Receiving user hasn't allowed file transfer\n");
        p_response->retcode = RETCODE_UNALLOWED;
        goto cleanup;
    }

    p_msg = calloc(g_max_packet_size, sizeof(*p_msg));
    if (NULL == p_msg)
    {
        fprintf(stderr, "calloc failed in opcode_file_send\n");
        p_response->retcode = RETCODE_FAILURE;
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_recv_hdr = (file_recv_hdr_t *)p_msg;

    p_recv_hdr->filename_size = htons(filename_size);
    p_recv_hdr->file_size     = htons(file_size);

    msg_size += sizeof(*p_recv_hdr);

    memcpy(p_msg + msg_size, p_filename, filename_size);
    msg_size += filename_size;

    memcpy(p_msg + msg_size, p_file, file_size);
    msg_size += file_size;

    msg_send(p_target, MSG_FLAG_FILE, p_msg, msg_size);
    memset(p_target->p_user_allow, 0, g_username_size_max);
    p_target->user_allow_size = 0u;

    if (p_server->b_verbose)
    {
        printf(
            "%.*s sent file \"%.*s\" to user: %.*s\n",
            p_session->username_size,
            p_session->p_username,
            filename_size,
            p_filename,
            p_target->username_size,
            p_target->p_username
        );
    }

cleanup:
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
opcode_promote (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t     * p_appdata        = NULL;
    sll_t         * p_admins         = NULL;
    sll_t         * p_room_store     = NULL;
    room_t        * p_room           = NULL;
    int             sockfd           = -1;
    server_t      * p_server         = NULL;
    uint8_t       * p_request_packet = NULL;
    uint16_t        username_size    = 0u;
    uint8_t       * p_username       = NULL;
    bool            b_locked         = false;
    promote_hdr_t * p_hdr            = NULL;

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
    p_room           = p_session->p_room;
    p_request_packet = p_request->p_packet;
    p_appdata        = p_server->p_appdata;
    p_room_store     = p_appdata->p_room_store;
    p_admins         = p_appdata->p_admins;

    if (NULL == p_room_store)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_PROMOTE;
    p_response->retcode = RETCODE_SUCCESS;

    p_hdr = (promote_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    username_size          = ntohs(p_hdr->username_size);
    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    if ((p_request->size + username_size) > g_max_packet_size)
    {
        fprintf(stderr, "Promote request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, username_size, g_chunk_size);
        p_response->retcode = RETCODE_OVERFLOW;
        goto cleanup;
    }

    p_username       = p_request_packet + p_request->size;
    p_request->size += username_size;

    sockutil_recvall(sockfd, p_username, username_size);

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    if (!is_admin(p_session, p_appdata))
    {
        p_response->retcode = RETCODE_UNAUTHORIZED;
        goto cleanup;
    }

    // Append user to admin list
    sll_append(p_admins, p_username, username_size);

    msg_send_username(
        p_room,
        p_username,
        username_size,
        MSG_FLAG_NOTIF,
        (uint8_t *)"You are now an admin",
        20u
    );

    if (p_server->b_verbose)
    {
        printf("Promoted %.*s to admin\n", username_size, p_username);
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
opcode_disconnect (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t        * p_appdata        = NULL;
    sll_t            * p_room_store     = NULL;
    room_t           * p_room           = NULL;
    int                sockfd           = -1;
    server_t         * p_server         = NULL;
    uint8_t          * p_request_packet = NULL;
    uint16_t           username_size    = 0u;
    uint8_t          * p_username       = NULL;
    session_t        * p_target         = NULL;
    bool               b_locked         = false;
    disconnect_hdr_t * p_hdr            = NULL;

    if (
        (NULL == p_session) ||
        (NULL == p_request) ||
        (NULL == p_response) ||
        (NULL == p_session->p_room) ||
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
    p_room           = p_session->p_room;
    p_request_packet = p_request->p_packet;
    p_appdata        = p_server->p_appdata;
    p_room_store     = p_appdata->p_room_store;

    if (NULL == p_room_store)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_DISCONNECT;
    p_response->retcode = RETCODE_SUCCESS;

    p_hdr = (disconnect_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    username_size          = ntohs(p_hdr->username_size);
    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    if ((p_request->size + username_size) > g_max_packet_size)
    {
        fprintf(stderr, "Disconnect request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, username_size, g_chunk_size);
        p_response->retcode = RETCODE_OVERFLOW;
        goto cleanup;
    }

    p_username       = p_request_packet + p_request->size;
    p_request->size += username_size;

    sockutil_recvall(sockfd, p_username, username_size);

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    if (!is_admin(p_session, p_appdata))
    {
        p_response->retcode = RETCODE_UNAUTHORIZED;
        goto cleanup;
    }

    msg_send_username(
        p_room,
        p_username,
        username_size,
        MSG_FLAG_NOTIF,
        (uint8_t *)"Disconnected by an admin",
        24u
    );

    p_target = session_by_username(p_room, p_username, username_size);
    if (NULL == p_target)
    {
        fprintf(
            stderr,
            "User %.*s is not in room\n",
            username_size,
            p_username
        );
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    user_logout(p_target, p_appdata);

    pthread_mutex_unlock(&(p_appdata->lock));
    b_locked = false;

    if (p_server->b_verbose)
    {
        printf("Disconnecting %.*s...\n", username_size, p_username);
    }

    client_destroy(p_server, p_target->p_client);

cleanup:
    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
        b_locked = false;
    }
    return status;
}

status_t
opcode_delete (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t    * p_appdata        = NULL;
    sll_t        * p_room_store     = NULL;
    room_t       * p_room           = NULL;
    node_t       * p_node           = NULL;
    int            sockfd           = -1;
    server_t     * p_server         = NULL;
    uint8_t      * p_request_packet = NULL;
    uint16_t       room_name_size   = 0u;
    uint8_t      * p_room_name      = NULL;
    bool           b_locked         = false;
    delete_hdr_t * p_hdr            = NULL;

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

    p_response->opcode  = OPCODE_DELETE;
    p_response->retcode = RETCODE_SUCCESS;

    p_hdr = (delete_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    room_name_size         = ntohs(p_hdr->room_name_size);
    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    if ((p_request->size + room_name_size) > g_max_packet_size)
    {
        fprintf(stderr, "Delete request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, room_name_size, g_chunk_size);
        p_response->retcode = RETCODE_OVERFLOW;
        goto cleanup;
    }

    p_room_name      = p_request_packet + p_request->size;
    p_request->size += room_name_size;

    sockutil_recvall(sockfd, p_room_name, room_name_size);

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    if (!is_admin(p_session, p_appdata))
    {
        p_response->retcode = RETCODE_UNAUTHORIZED;
        goto cleanup;
    }

    p_node = sll_get(p_room_store, p_room_name, room_name_size);
    if (NULL == p_node)
    {
        fprintf(
            stderr,
            "Room doesn't exist: %.*s\n",
            room_name_size,
            p_room_name
        );
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    p_room = *(room_t **)(p_node->p_data);

    msg_send_room(
        p_room,
        MSG_FLAG_NOTIF,
        (uint8_t *)"Current room deleted by admin",
        29u
    );

    msg_send_room(
        p_room,
        MSG_FLAG_JOIN,
        (uint8_t *)"",
        0u
    );

    if (p_server->b_verbose)
    {
        printf("Deleting room: %.*s\n", room_name_size, p_room_name);
    }

    room_destroy(p_room);
    p_room = NULL;

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

static status_t
msg_send (
    session_t     * p_session,
    uint8_t         flag,
    uint8_t const * p_msg,
    uint16_t        msg_size
)
{
    status_t status = STATUS_SUCCESS;

    uint8_t        * p_packet = NULL;
    msg_recv_hdr_t   hdr      = {0};

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
    hdr.flag     = flag;
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

static status_t
msg_send_username (
    room_t        * p_room,
    uint8_t       * p_username,
    uint16_t        username_size,
    uint8_t         flag,
    uint8_t const * p_msg,
    uint16_t        msg_size
)
{
    status_t status = STATUS_SUCCESS;

    node_t    * p_curr    = NULL;
    session_t * p_session = NULL;

    if (
        (NULL == p_room) ||
        (NULL == p_room->p_sessions) ||
        (NULL == p_username) ||
        (NULL == p_msg)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_curr = p_room->p_sessions->p_head;
    while (NULL != p_curr)
    {
        p_session = *(session_t **)(p_curr->p_data);
        if (
            (p_session->username_size == username_size) &&
            (0 == memcmp(p_session->p_username, p_username, username_size))
        )
        {
            msg_send(p_session, flag, p_msg, msg_size);
        }

        p_curr = p_curr->p_next;
    }

cleanup:
    return status;
}

static status_t
msg_send_room (
    room_t        * p_room,
    uint8_t         flag,
    uint8_t const * p_msg,
    uint16_t        msg_size
)
{
    status_t status = STATUS_SUCCESS;

    node_t * p_curr = NULL;

    if (
        (NULL == p_room) ||
        (NULL == p_room->p_sessions) ||
        (NULL == p_msg)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_curr = p_room->p_sessions->p_head;
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

        msg_send(p_target, flag, p_msg, msg_size);

        p_curr = p_curr->p_next;
    }

cleanup:
    return status;
}

static bool
is_admin (session_t * p_session, appdata_t * p_appdata)
{
    bool       b_is_admin    = false;
    uint16_t   username_size = 0u;
    uint8_t  * p_username    = NULL;

    if (
        (NULL == p_session) ||
        (NULL == p_session->p_username) ||
        (NULL == p_appdata) ||
        (NULL == p_appdata->p_admins)
    )
    {
        goto cleanup;
    }

    username_size = p_session->username_size;
    p_username    = p_session->p_username;

    printf("Checking if %.*s is an admin...\n", username_size, p_username);

    if (NULL != sll_get(p_appdata->p_admins, p_username, username_size))
    {
        b_is_admin = true;
    }

cleanup:
    return b_is_admin;
}

/*** end of file ***/
