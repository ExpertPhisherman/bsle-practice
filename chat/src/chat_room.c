/** @file chat_room.c
 *
 * @brief Room opcodes source: join, list
 *
 * @par
 *
 */

#include "chat_room.h"

extern uint32_t const g_max_packet_size;
extern uint32_t const g_chunk_size;

status_t
opcode_join (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t * p_appdata        = NULL;
    sll_t     * p_room_store     = NULL;
    node_t    * p_node           = NULL;
    int         sockfd           = -1;
    server_t  * p_server         = NULL;
    uint8_t   * p_request_packet = NULL;
    room_t    * p_room           = NULL;
    uint8_t   * p_room_name      = NULL;
    uint16_t    room_name_size   = 0u;
    bool        b_locked         = false;

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

    sockutil_recvall(sockfd, p_request_packet + p_request->size, room_name_size);

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

    appdata_t  * p_appdata        = NULL;
    sll_t      * p_room_store     = NULL;
    room_t     * p_room           = NULL;
    node_t     * p_curr           = NULL;
    session_t  * p_target         = NULL;
    int          sockfd           = -1;
    server_t   * p_server         = NULL;
    uint8_t    * p_request_packet = NULL;
    uint8_t      flag             = 0u;
    bool         b_locked         = false;
    list_hdr_t * p_hdr            = NULL;

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

    p_hdr = (list_hdr_t *)(p_request_packet + p_request->size);

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
                msg_send(p_session, MSG_FLAG_LIST, p_room->p_name, p_room->name_size);
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

/*** end of file ***/
