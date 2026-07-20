/** @file chat_admin.c
 *
 * @brief Admin opcodes source: promote, disconnect, delete
 *
 * @par
 *
 */

#include "chat_admin.h"
#include "chat_internal.h"
#include "server.h"
#include "client.h"

extern uint32_t const g_max_packet_size;
extern uint32_t const g_chunk_size;

bool
is_admin (session_t * p_session, appdata_t * p_appdata)
{
    bool b_is_admin = false;

    if (
        (NULL == p_session) ||
        (NULL == p_session->p_username) ||
        (NULL == p_appdata) ||
        (NULL == p_appdata->p_admins)
    )
    {
        goto cleanup;
    }

    uint16_t   username_size = p_session->username_size;
    uint8_t  * p_username    = p_session->p_username;

    if (NULL != p_session->p_server)
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

    appdata_t * p_appdata = NULL;
    bool        b_locked  = false;

    if (!opcode_args_valid(p_session, p_request, p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    int         sockfd       = p_session->sockfd;
    server_t  * p_server     = p_session->p_server;
    uint8_t   * p_req_packet = p_request->p_packet;
    p_appdata                = p_server->p_appdata;

    promote_hdr_t * p_hdr = (promote_hdr_t *)(p_req_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    uint16_t username_size  = ntohs(p_hdr->username_size);
    p_request->session_id   = ntohl(p_hdr->session_id);
    p_request->size        += sizeof(*p_hdr);

    if ((p_request->size + username_size) > g_max_packet_size)
    {
        fprintf(stderr, "Promote request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, username_size, g_chunk_size);
        p_response->retcode = RETCODE_OVERFLOW;
        goto cleanup;
    }

    uint8_t * p_username  = p_req_packet + p_request->size;
    p_request->size      += username_size;

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

    if (NULL != ht_get(p_appdata->p_admins, p_username, username_size))
    {
        printf("User %.*s is already admin\n", username_size, p_username);
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    ht_set(p_appdata->p_admins, p_username, username_size, "", 0u);

    session_t * p_target = session_get(
        p_username,
        username_size,
        p_appdata->p_session_store
    );
    if (NULL != p_target)
    {
        msg_send(
            p_target,
            MSG_FLAG_NOTIF,
            (uint8_t *)"You are now an admin",
            20u
        );
    }

    printf("Promoted %.*s to admin\n", username_size, p_username);

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

    appdata_t * p_appdata = NULL;
    bool        b_locked  = false;

    if (!opcode_args_valid(p_session, p_request, p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    int         sockfd       = p_session->sockfd;
    server_t  * p_server     = p_session->p_server;
    uint8_t   * p_req_packet = p_request->p_packet;
    p_appdata                = p_server->p_appdata;

    disconnect_hdr_t * p_hdr = (disconnect_hdr_t *)(p_req_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    uint16_t username_size  = ntohs(p_hdr->username_size);
    p_request->session_id   = ntohl(p_hdr->session_id);
    p_request->size        += sizeof(*p_hdr);

    if ((p_request->size + username_size) > g_max_packet_size)
    {
        fprintf(stderr, "Disconnect request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, username_size, g_chunk_size);
        p_response->retcode = RETCODE_OVERFLOW;
        goto cleanup;
    }

    uint8_t * p_username  = p_req_packet + p_request->size;
    p_request->size      += username_size;

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

    item_t    * p_item   = ht_get(p_appdata->p_session_store, p_username, username_size);
    session_t * p_target = (NULL != p_item) ? *(session_t **)(p_item->p_value) : NULL;

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

    printf("Disconnecting %.*s...\n", username_size, p_username);

    // Handle the case where an admin disconnects themself
    if (p_target != p_session)
    {
        client_destroy(p_target->p_client);
    }
    else
    {
        status = STATUS_CLIENT_DISCONNECT;
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
opcode_delete (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t * p_appdata = NULL;
    bool        b_locked  = false;

    if (!opcode_args_valid(p_session, p_request, p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    int         sockfd       = p_session->sockfd;
    server_t  * p_server     = p_session->p_server;
    uint8_t   * p_req_packet = p_request->p_packet;
    p_appdata                = p_server->p_appdata;

    sll_t * p_room_store = p_appdata->p_room_store;

    delete_hdr_t * p_hdr = (delete_hdr_t *)(p_req_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    uint16_t room_name_size  = ntohs(p_hdr->room_name_size);
    p_request->session_id    = ntohl(p_hdr->session_id);
    p_request->size         += sizeof(*p_hdr);

    if ((p_request->size + room_name_size) > g_max_packet_size)
    {
        fprintf(stderr, "Delete request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, room_name_size, g_chunk_size);
        p_response->retcode = RETCODE_OVERFLOW;
        goto cleanup;
    }

    uint8_t * p_room_name  = p_req_packet + p_request->size;
    p_request->size       += room_name_size;

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

    room_t probe =
    {
        .p_name    = p_room_name,
        .name_size = room_name_size,
    };

    room_t * p_probe = &probe;

    node_t * p_node = sll_get(p_room_store, &p_probe, sizeof(p_probe));
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

    room_t * p_room = *(room_t **)(p_node->p_data);

    msg_send_room(
        p_room,
        MSG_FLAG_NOTIF,
        (uint8_t *)"Current room deleted by admin",
        29u
    );

    msg_send_room(p_room, MSG_FLAG_JOIN, (uint8_t *)"", 0u);

    node_t * p_curr = p_room->p_sessions->p_head;
    while (NULL != p_curr)
    {
        node_t    * p_next   = p_curr->p_next;
        session_t * p_member = *(session_t **)(p_curr->p_data);
        user_leave(p_member, p_appdata);
        p_curr = p_next;
    }

    printf("Deleting room: %.*s\n", room_name_size, p_room_name);

    sll_remove(p_room_store, &p_probe, sizeof(p_probe));

cleanup:
    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
        b_locked = false;
    }
    return status;
}

/*** end of file ***/
