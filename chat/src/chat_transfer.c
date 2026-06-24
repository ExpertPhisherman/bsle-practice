/** @file chat_transfer.c
 *
 * @brief Transfer opcodes source: request, respond, file_send
 *
 * @par
 *
 */

#include "chat_transfer.h"

extern uint32_t const g_max_packet_size;
extern uint32_t const g_chunk_size;
extern uint16_t const g_username_size_max;

/*!
 * @brief Send "Responded: decline" to self and a notification to the requester
 *
 * @param[in] p_session          Pointer to session
 * @param[in] p_session_store    Pointer to session storage
 * @param[in] p_username         Pointer to username
 * @param[in] username_size      Size of username in bytes
 * @param[in] p_decline_notif    Pointer to notification message
 * @param[in] decline_notif_size Size of notification message in bytes
 *
 * @return Status of operation
 */
static status_t respond_decline(
    session_t     * p_session,
    ht_t          * p_session_store,
    uint8_t       * p_username,
    uint16_t        username_size,
    uint8_t const * p_decline_notif,
    uint16_t        decline_notif_size
);

/*!
 * @brief Accept PM request: create private room and move both users into it
 *
 * @param[in] p_session       Pointer to session
 * @param[in] p_appdata       Pointer to application data
 * @param[in] p_session_store Pointer to session storage
 * @param[in] p_username      Pointer to username
 * @param[in] username_size   Size of username in bytes
 *
 * @return Status of operation
 */
static status_t respond_pm_accept(
    session_t * p_session,
    appdata_t * p_appdata,
    ht_t      * p_session_store,
    uint8_t   * p_username,
    uint16_t    username_size
);

/*!
 * @brief Relay a file to the target session and clear their user_allow
 *
 * @param[in] p_sender      Pointer to sender session
 * @param[in] p_target      Pointer to target session
 * @param[in] p_filename    Pointer to filename
 * @param[in] filename_size Size of filename in bytes
 * @param[in] p_file        Pointer to file content
 * @param[in] file_size     Size of file content in bytes
 *
 * @return Status of operation
 */
static status_t file_relay(
    session_t     * p_sender,
    session_t     * p_target,
    uint8_t const * p_filename,
    uint16_t        filename_size,
    uint8_t const * p_file,
    uint32_t        file_size
);

static status_t
respond_decline (
    session_t     * p_session,
    ht_t          * p_session_store,
    uint8_t       * p_username,
    uint16_t        username_size,
    uint8_t const * p_decline_notif,
    uint16_t        decline_notif_size
)
{
    msg_send(p_session, MSG_FLAG_NOTIF, (uint8_t *)"Responded: decline", 18u);
    msg_send_username(
        p_username,
        username_size,
        p_session_store,
        MSG_FLAG_NOTIF,
        p_decline_notif,
        decline_notif_size
    );
    return STATUS_SUCCESS;
}

static status_t
respond_pm_accept (
    session_t * p_session,
    appdata_t * p_appdata,
    ht_t      * p_session_store,
    uint8_t   * p_username,
    uint16_t    username_size
)
{
    status_t status = STATUS_SUCCESS;

    room_t    * p_new_room     = NULL;
    uint8_t   * p_room_name    = NULL;
    uint8_t   * p_user1        = NULL;
    uint16_t    user1_size     = 0u;
    uint8_t   * p_user2        = NULL;
    uint16_t    user2_size     = 0u;
    uint16_t    room_name_size = 0u;
    session_t * p_target       = NULL;
    sll_t     * p_room_store   = p_appdata->p_room_store;

    p_room_name = calloc(2u * g_username_size_max + 1u, sizeof(*p_room_name));
    if (NULL == p_room_name)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Assign usernames to variables alphabetically
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
        p_user1    = p_username;
        user1_size = username_size;
        p_user2    = p_session->p_username;
        user2_size = p_session->username_size;
    }
    else
    {
        p_user1    = p_session->p_username;
        user1_size = p_session->username_size;
        p_user2    = p_username;
        user2_size = username_size;
    }

    memcpy(p_room_name, p_user1, user1_size);
    p_room_name[user1_size] = '+';
    memcpy(p_room_name + user1_size + 1u, p_user2, user2_size);
    room_name_size = user1_size + 1u + user2_size;

    p_new_room = room_create((char *)p_room_name, room_name_size);
    if (NULL == p_new_room)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    status = sll_append(p_room_store, &p_new_room, sizeof(p_new_room));
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "sll_append failed in respond_pm_accept\n");
        room_destroy(p_new_room);
        p_new_room = NULL;
        goto cleanup;
    }

    user_leave(p_session, p_appdata);
    p_session->p_room = p_new_room;
    user_join(p_session, p_appdata);
    msg_send(p_session, MSG_FLAG_JOIN, p_room_name, room_name_size);

    p_target = session_get(p_username, username_size, p_session_store);
    user_leave(p_target, p_appdata);
    p_target->p_room = p_new_room;
    user_join(p_target, p_appdata);
    msg_send(p_target, MSG_FLAG_JOIN, p_room_name, room_name_size);

cleanup:
    free(p_room_name);
    p_room_name = NULL;
    return status;
}

static status_t
file_relay (
    session_t     * p_sender,
    session_t     * p_target,
    uint8_t const * p_filename,
    uint16_t        filename_size,
    uint8_t const * p_file,
    uint32_t        file_size
)
{
    status_t status = STATUS_SUCCESS;

    uint8_t         * p_msg      = NULL;
    uint16_t          msg_size   = 0u;
    file_recv_hdr_t * p_recv_hdr = NULL;

    p_msg = calloc(
        sizeof(*p_recv_hdr) + filename_size + file_size,
        sizeof(*p_msg)
    );
    if (NULL == p_msg)
    {
        fprintf(stderr, "calloc failed in file_relay\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_recv_hdr                = (file_recv_hdr_t *)p_msg;
    p_recv_hdr->filename_size = htons(filename_size);
    p_recv_hdr->file_size     = htonl(file_size);
    msg_size                  = sizeof(*p_recv_hdr);

    memcpy(p_msg + msg_size, p_filename, filename_size);
    msg_size += filename_size;

    memcpy(p_msg + msg_size, p_file, file_size);
    msg_size += (uint16_t)file_size;

    msg_send(p_target, MSG_FLAG_FILE, p_msg, msg_size);

    memset(p_target->p_user_allow, 0, g_username_size_max);
    p_target->user_allow_size = 0u;

    if (p_sender->p_server->b_verbose)
    {
        printf(
            "%.*s sent file \"%.*s\" to user: %.*s\n",
            p_sender->username_size,
            p_sender->p_username,
            filename_size,
            p_filename,
            p_target->username_size,
            p_target->p_username
        );
    }

cleanup:
    free(p_msg);
    p_msg = NULL;
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
    uint8_t   * p_notif          = NULL;
    int         written          = -1;
    bool        b_locked         = false;
    req_hdr_t * p_hdr            = NULL;

    if (!opcode_args_valid(p_session, p_request, p_response))
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

    p_notif = calloc(g_max_packet_size, sizeof(*p_notif));
    if (NULL == p_notif)
    {
        fprintf(stderr, "calloc failed in opcode_request\n");
        p_response->retcode = RETCODE_FAILURE;
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_hdr = (req_hdr_t *)(p_request_packet + p_request->size);

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

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    if (NULL == session_get(
        p_username,
        username_size,
        p_appdata->p_session_store
    ))
    {
        fprintf(stderr, "Receiving user has no session\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    switch (flag_type)
    {
        case REQ_FLAG_TYPE_PM:
        {
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
                (char *)p_notif,
                g_max_packet_size,
                "PM request received from %.*s",
                p_session->username_size,
                p_session->p_username
            );

            msg_send_username(
                p_username,
                username_size,
                p_appdata->p_session_store,
                MSG_FLAG_NOTIF,
                p_notif,
                (uint16_t)written
            );

            break;
        }

        case REQ_FLAG_TYPE_FILE:
        {
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
                (char *)p_notif,
                g_max_packet_size,
                "File transfer request received from %.*s",
                p_session->username_size,
                p_session->p_username
            );

            msg_send_username(
                p_username,
                username_size,
                p_appdata->p_session_store,
                MSG_FLAG_NOTIF,
                p_notif,
                (uint16_t)written
            );

            break;
        }

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
    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
        b_locked = false;
    }
    free(p_notif);
    p_notif = NULL;
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

    appdata_t  * p_appdata        = NULL;
    sll_t      * p_room_store     = NULL;
    room_t     * p_room           = NULL;
    item_t     * p_item           = NULL;
    int          sockfd           = -1;
    server_t   * p_server         = NULL;
    uint8_t    * p_request_packet = NULL;
    ht_t       * p_pm_reqs        = NULL;
    ht_t       * p_file_reqs      = NULL;
    uint8_t      flag_type        = 0u;
    uint8_t      flag_choice      = 0u;
    uint16_t     username_size    = 0u;
    uint8_t    * p_username       = NULL;
    uint8_t    * p_notif          = NULL;
    int          written          = -1;
    bool         b_locked         = false;
    resp_hdr_t * p_hdr            = NULL;

    if (!opcode_args_valid(p_session, p_request, p_response))
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

    p_notif = calloc(g_max_packet_size, sizeof(*p_notif));
    if (NULL == p_notif)
    {
        fprintf(stderr, "calloc failed in opcode_respond\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    p_hdr = (resp_hdr_t *)(p_request_packet + p_request->size);

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

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    if (NULL == session_get(
        p_username,
        username_size,
        p_appdata->p_session_store
    ))
    {
        fprintf(stderr, "Receiving has no session\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    switch (flag_type)
    {
        case RESP_FLAG_TYPE_PM:
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

            if (RESP_FLAG_CHOICE_DECLINE == flag_choice)
            {
                written = snprintf(
                    (char *)p_notif,
                    g_max_packet_size,
                    "PM request declined by %.*s",
                    p_session->username_size,
                    p_session->p_username
                );
                respond_decline(
                    p_session,
                    p_appdata->p_session_store,
                    p_username,
                    username_size,
                    p_notif,
                    (uint16_t)written
                );
            }
            else
            {
                status = respond_pm_accept(
                    p_session,
                    p_appdata,
                    p_appdata->p_session_store,
                    p_username,
                    username_size
                );
                if (STATUS_SUCCESS != status)
                {
                    p_response->retcode = RETCODE_FAILURE;
                    goto cleanup;
                }
            }

            ht_set(
                p_room->p_pm_reqs,
                p_session->p_username,
                p_session->username_size,
                "",
                0u
            );

            break;

        case RESP_FLAG_TYPE_FILE:
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

            if (RESP_FLAG_CHOICE_DECLINE == flag_choice)
            {
                written = snprintf(
                    (char *)p_notif,
                    g_max_packet_size,
                    "File transfer request declined by %.*s",
                    p_session->username_size,
                    p_session->p_username
                );
                respond_decline(
                    p_session,
                    p_appdata->p_session_store,
                    p_username,
                    username_size,
                    p_notif,
                    (uint16_t)written
                );
            }
            else
            {
                memcpy(p_session->p_user_allow, p_username, username_size);
                p_session->user_allow_size = username_size;
            }

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
    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
        b_locked = false;
    }
    free(p_notif);
    p_notif = NULL;
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
    uint32_t          file_size        = 0u;
    uint8_t         * p_username       = NULL;
    uint8_t         * p_filename       = NULL;
    uint8_t         * p_file           = NULL;
    session_t       * p_target         = NULL;
    bool              b_locked         = false;
    file_send_hdr_t * p_hdr            = NULL;

    if (!opcode_args_valid(p_session, p_request, p_response))
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

    p_hdr = (file_send_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    username_size          = ntohs(p_hdr->username_size);
    filename_size          = ntohs(p_hdr->filename_size);
    file_size              = ntohl(p_hdr->file_size);
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
            (username_size + filename_size + file_size),
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

    p_target = session_get(
        p_username,
        username_size,
        p_appdata->p_session_store
    );

    if (NULL == p_target)
    {
        fprintf(stderr, "Receiving user has no session\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

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

    status = file_relay(
        p_session,
        p_target,
        p_filename,
        filename_size,
        p_file,
        file_size
    );
    if (STATUS_SUCCESS != status)
    {
        p_response->retcode = RETCODE_FAILURE;
    }

cleanup:
    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
        b_locked = false;
    }
    return status;
}

/*** end of file ***/
