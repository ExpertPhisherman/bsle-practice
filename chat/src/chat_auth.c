/** @file chat_auth.c
 *
 * @brief Auth opcodes source: login, logout
 *
 * @par
 *
 */

#include "chat_auth.h"

extern uint32_t const g_max_packet_size;
extern uint32_t const g_chunk_size;
extern uint16_t const g_username_size_max;
extern uint16_t const g_password_size_max;

/*!
 * @brief Receive and validate login credentials from socket
 *
 * @param[in]  sockfd     Socket file descriptor
 * @param[in]  p_session  Pointer to session
 * @param[in]  p_request  Pointer to request
 * @param[out] p_response Pointer to response
 *
 * @return Status of operation
 */
static status_t login_recv_creds(
    int          sockfd,
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
);

status_t
opcode_login (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t * p_appdata  = NULL;
    int         sockfd     = -1;
    server_t  * p_server   = NULL;
    uint32_t    session_id = 0u;
    bool        b_locked   = false;

    if (!opcode_args_valid(p_session, p_request, p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd    = p_session->sockfd;
    p_server  = p_session->p_server;
    p_appdata = p_server->p_appdata;

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    user_logout(p_session, p_appdata);

    pthread_mutex_unlock(&(p_appdata->lock));
    b_locked = false;

    status = login_recv_creds(sockfd, p_session, p_request, p_response);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }
    if (RETCODE_SUCCESS != p_response->retcode)
    {
        goto cleanup;
    }

    if (p_server->b_verbose)
    {
        printf(
            "Attempting login with username=\"%.*s\", password=\"%.*s\"\n",
            p_session->username_size,
            p_session->p_username,
            p_session->password_size,
            p_session->p_password
        );
    }

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

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
        *(uint32_t *)(p_response->p_packet + p_response->size) = htonl(
            session_id
        );
        p_session->session_id = session_id;
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

    appdata_t    * p_appdata        = NULL;
    int            sockfd           = -1;
    server_t     * p_server         = NULL;
    uint8_t      * p_request_packet = NULL;
    bool           b_locked         = false;
    logout_hdr_t * p_hdr            = NULL;

    if (!opcode_args_valid(p_session, p_request, p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_server         = p_session->p_server;
    p_request_packet = p_request->p_packet;
    p_appdata        = p_server->p_appdata;

    p_hdr = (logout_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    p_request->session_id  = ntohl(p_hdr->session_id);
    p_request->size       += sizeof(*p_hdr);

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

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

static status_t
login_recv_creds (
    int          sockfd,
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    uint16_t      username_size    = 0u;
    uint16_t      password_size    = 0u;
    uint8_t     * p_username       = NULL;
    uint8_t     * p_password       = NULL;
    uint8_t     * p_request_packet = NULL;
    login_hdr_t * p_hdr            = NULL;

    if (!opcode_args_valid(p_session, p_request, p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_request_packet = p_request->p_packet;

    p_hdr = (login_hdr_t *)(p_request_packet + p_request->size);

    sockutil_recvall(sockfd, p_hdr, sizeof(*p_hdr));

    username_size = ntohs(p_hdr->username_size);
    password_size = ntohs(p_hdr->password_size);

    p_session->username_size = username_size;
    p_session->password_size = password_size;
    p_request->size         += sizeof(*p_hdr);

    if ((p_request->size + username_size + password_size) > g_max_packet_size)
    {
        fprintf(stderr, "Login request size exceeds g_max_packet_size\n");
        sockutil_drain(sockfd, username_size + password_size, g_chunk_size);
        p_response->retcode = RETCODE_OVERFLOW;
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

    if (!user_creds_valid(p_session))
    {
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

cleanup:
    return status;
}

/*** end of file ***/
