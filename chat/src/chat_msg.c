/** @file chat_msg.c
 *
 * @brief Message opcodes source: msg_send, msg_recv
 *
 * @par
 *
 */

#include "chat_msg.h"
#include "server.h"
#include "client.h"

extern uint32_t const g_max_packet_size;
extern uint32_t const g_chunk_size;

status_t
msg_send (
    session_t     * p_session,
    uint8_t         flag,
    uint8_t const * p_msg,
    uint16_t        msg_size
)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_session) || (NULL == p_msg))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    msg_recv_hdr_t hdr = {0};

    uint8_t * p_packet = calloc(sizeof(hdr) + msg_size, sizeof(*p_packet));
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

status_t
msg_send_username (
    uint8_t       * p_username,
    uint16_t        username_size,
    ht_t          * p_session_store,
    uint8_t         flag,
    uint8_t const * p_msg,
    uint16_t        msg_size
)
{
    status_t status = STATUS_SUCCESS;

    if (
        (NULL == p_username) ||
        (NULL == p_session_store) ||
        (NULL == p_msg)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    session_t * p_session = session_get(p_username, username_size, p_session_store);
    msg_send(p_session, flag, p_msg, msg_size);

cleanup:
    return status;
}

status_t
msg_send_room (
    room_t        * p_room,
    uint8_t         flag,
    uint8_t const * p_msg,
    uint16_t        msg_size
)
{
    status_t status = STATUS_SUCCESS;

    if (
        (NULL == p_room) ||
        (NULL == p_room->p_sessions) ||
        (NULL == p_msg)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    node_t * p_curr = p_room->p_sessions->p_head;
    while (NULL != p_curr)
    {
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

status_t
opcode_msg_send (
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

    msg_send_hdr_t * p_hdr = (msg_send_hdr_t *)(p_req_packet + p_request->size);

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

    uint8_t * p_msg = p_req_packet + p_request->size;
    p_request->size += msg_size;

    sockutil_recvall(sockfd, p_msg, msg_size);

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    room_t * p_room = p_session->p_room;

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

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

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

/*** end of file ***/
