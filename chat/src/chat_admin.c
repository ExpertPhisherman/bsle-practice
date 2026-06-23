/** @file chat_admin.c
 *
 * @brief Admin opcodes source: promote, disconnect, delete
 *
 * @par
 *
 */

#include "chat_admin.h"

extern uint32_t const g_max_packet_size;
extern uint32_t const g_chunk_size;

bool
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

    if (
        (NULL != p_session->p_server) &&
        (p_session->p_server->b_verbose)
    )
    {
        printf("Checking if %.*s is an admin...\n", username_size, p_username);
    }

    if (NULL != ht_get(p_appdata->p_admins, p_username, username_size))
    {
        b_is_admin = true;
    }

cleanup:
    return b_is_admin;
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
    ht_t          * p_admins         = NULL;
    item_t        * p_item           = NULL;
    session_t     * p_target         = NULL;
    int             sockfd           = -1;
    server_t      * p_server         = NULL;
    uint8_t       * p_request_packet = NULL;
    uint16_t        username_size    = 0u;
    uint8_t       * p_username       = NULL;
    bool            b_locked         = false;
    promote_hdr_t * p_hdr            = NULL;

    if (!opcode_args_valid(p_session, p_request, p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_server         = p_session->p_server;
    p_request_packet = p_request->p_packet;
    p_appdata        = p_server->p_appdata;
    p_admins         = p_appdata->p_admins;

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

    ht_set(p_admins, p_username, username_size, "", 0u);

    p_item   = ht_get(p_appdata->p_session_store, p_username, username_size);
    p_target = (NULL != p_item) ? *(session_t **)(p_item->p_value) : NULL;
    if (NULL != p_target)
    {
        msg_send(
            p_target,
            MSG_FLAG_NOTIF,
            (uint8_t *)"You are now an admin",
            20u
        );
    }

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
    int                sockfd           = -1;
    server_t         * p_server         = NULL;
    uint8_t          * p_request_packet = NULL;
    uint16_t           username_size    = 0u;
    uint8_t          * p_username       = NULL;
    item_t           * p_item           = NULL;
    session_t        * p_target         = NULL;
    bool               b_locked         = false;
    disconnect_hdr_t * p_hdr            = NULL;

    if (!opcode_args_valid(p_session, p_request, p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_server         = p_session->p_server;
    p_request_packet = p_request->p_packet;
    p_appdata        = p_server->p_appdata;

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

    p_item   = ht_get(p_appdata->p_session_store, p_username, username_size);
    p_target = (NULL != p_item) ? *(session_t **)(p_item->p_value) : NULL;

    if (NULL == p_target)
    {
        fprintf(
            stderr,
            "User %.*s is not online\n",
            username_size,
            p_username
        );
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    msg_send(
        p_target,
        MSG_FLAG_NOTIF,
        (uint8_t *)"Disconnected by an admin",
        24u
    );

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
    node_t       * p_curr           = NULL;
    node_t       * p_next           = NULL;
    session_t    * p_member         = NULL;
    int            sockfd           = -1;
    server_t     * p_server         = NULL;
    uint8_t      * p_request_packet = NULL;
    uint16_t       room_name_size   = 0u;
    uint8_t      * p_room_name      = NULL;
    bool           b_locked         = false;
    delete_hdr_t * p_hdr            = NULL;

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

    msg_send_room(p_room, MSG_FLAG_JOIN, (uint8_t *)"", 0u);

    p_curr = p_room->p_sessions->p_head;
    while (NULL != p_curr)
    {
        p_next   = p_curr->p_next;
        p_member = *(session_t **)(p_curr->p_data);
        user_leave(p_member, p_appdata);
        p_curr = p_next;
    }

    if (p_server->b_verbose)
    {
        printf("Deleting room: %.*s\n", room_name_size, p_room_name);
    }

    sll_remove(p_room_store, p_room_name, room_name_size);
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

/*** end of file ***/
