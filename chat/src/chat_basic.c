/** @file chat_basic.c
 *
 * @brief Basic opcodes source: default, ping, echo, quit
 *
 * @par
 *
 */

#include "chat_basic.h"

extern uint32_t const g_max_packet_size;
extern uint32_t const g_chunk_size;

bool
opcode_args_valid (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    return (
        (NULL != p_session) &&
        (NULL != p_request) &&
        (NULL != p_response) &&
        (NULL != p_session->p_server) &&
        (NULL != p_session->p_server->p_appdata) &&
        (NULL != p_request->p_packet) &&
        (NULL != p_response->p_packet)
    );
}

status_t
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

    int          sockfd           = -1;
    uint8_t    * p_request_packet = NULL;
    ping_hdr_t * p_hdr            = NULL;

    if (!opcode_args_valid(p_session, p_request, p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_request_packet = p_request->p_packet;

    p_hdr = (ping_hdr_t *)(p_request_packet + p_request->size);

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

    int          sockfd            = -1;
    uint8_t    * p_request_packet  = NULL;
    uint8_t    * p_response_packet = NULL;
    uint16_t     payload_size      = 0u;
    echo_hdr_t * p_hdr             = NULL;

    if (!opcode_args_valid(p_session, p_request, p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_request_packet  = p_request->p_packet;
    p_response_packet = p_response->p_packet;

    p_hdr = (echo_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    payload_size = ntohs(p_hdr->payload_size);
    p_request->session_id = ntohl(p_hdr->session_id);

    // Copy size field into response
    *(uint16_t *)(p_response_packet + p_response->size) = htons(payload_size);

    p_request->size += sizeof(*p_hdr);

    if ((p_request->size + payload_size) > g_max_packet_size)
    {
        p_response->retcode = RETCODE_OVERFLOW;
        fprintf(stderr, "Echo request size exceeds g_max_packet_size\n");
        memset(p_response_packet + p_response->size, 0, FIELD_SIZE_SIZE);
        sockutil_drain(sockfd, payload_size, g_chunk_size);
        payload_size = 0u;
    }

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        memset(p_response_packet + p_response->size, 0, FIELD_SIZE_SIZE);
        sockutil_drain(sockfd, payload_size, g_chunk_size);
        payload_size = 0u;
    }

    p_response->size += FIELD_SIZE_SIZE;

    sockutil_recvall(sockfd, p_request_packet + p_request->size, payload_size);

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

    int          sockfd           = -1;
    server_t   * p_server         = NULL;
    uint8_t    * p_request_packet = NULL;
    quit_hdr_t * p_hdr            = NULL;

    if (!opcode_args_valid(p_session, p_request, p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_server         = p_session->p_server;
    p_request_packet = p_request->p_packet;

    p_hdr = (quit_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    p_request->size += sizeof(*p_hdr);

    if (p_server->b_verbose)
    {
        printf("Quitting client session on sockfd %d\n", p_session->sockfd);
    }

cleanup:
    return status;
}

/*** end of file ***/
