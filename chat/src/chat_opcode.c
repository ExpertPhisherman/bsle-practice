/** @file chat_opcode.c
 *
 * @brief Chat opcode source
 *
 * @par
 *
 */

#include "chat_opcode.h"

extern uint32_t const max_packet_size;
extern uint32_t const chunk_size;

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

status_t
opcode_default (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    int         sockfd            = -1;
    server_t  * p_server          = NULL;
    uint8_t   * p_request_packet  = NULL;
    uint8_t   * p_response_packet = NULL;

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

    p_response->retcode = RETCODE_FAILURE;

    p_response->size = 1u;

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

    // Receive header
    p_request->size = 6u;
    sockutil_recvall(sockfd, p_request_packet + 1u, p_request->size - 1u);

    p_request->session_id = ntohl(*(uint32_t *)(p_request_packet + 2u));

    p_response->size = 1u;

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    p_response->retcode = RETCODE_SUCCESS;

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

    // Receive header
    p_request->size = 8u;
    sockutil_recvall(sockfd, p_request_packet + 1u, p_request->size - 1u);

    p_request->size += ntohs(*(uint16_t *)(p_request_packet + 2u));

    p_request->session_id = ntohl(*(uint32_t *)(p_request_packet + 4u));

    if (p_request->size > max_packet_size)
    {
        p_response->retcode = RETCODE_FAILURE;
        fprintf(stderr, "Echo request size exceeds max_packet_size\n");
        sockutil_drain(sockfd, p_request->size - 8u, chunk_size);
        p_response->size = 2u;
        goto cleanup;
    }

    // Receive payload
    sockutil_recvall(sockfd, p_request_packet + 8u, p_request->size - 8u);

    p_response->size = 2u + p_request->size - 8u;

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        memset(p_response_packet + 2u, 0, p_request->size - 8u);
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    memcpy(p_response_packet + 2u, p_request_packet + 8u, p_request->size - 8u);

    p_response->retcode = RETCODE_SUCCESS;

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

    server_t * p_server          = NULL;
    uint8_t  * p_response_packet = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_server = p_session->p_server;
    p_response_packet = p_response->p_packet;

    if ((NULL == p_server) || (NULL == p_response_packet))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_request->size = 1u;

    p_response->retcode = RETCODE_SUCCESS;

    if (p_server->b_verbose)
    {
        printf("Quitting client session on sockfd %d\n", p_session->sockfd);
    }

    p_response->size = 1u;

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
    item_t    * p_item            = NULL;
    int         sockfd            = -1;
    server_t  * p_server          = NULL;
    uint8_t   * p_request_packet  = NULL;
    uint8_t   * p_response_packet = NULL;
    char      * p_tmp             = NULL;

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

    // Receive header
    p_request->size = 12u;
    sockutil_recvall(sockfd, p_request_packet + 1u, p_request->size - 1u);
    uint8_t user_flag = p_request_packet[1];
    UNUSED(user_flag);

    // NOTE: 2 byte reserved field between user flag and username length

    p_session->username_size = ntohs(*(uint16_t *)(p_request_packet + 4u));
    p_session->password_size = ntohs(*(uint16_t *)(p_request_packet + 6u));
    p_request->session_id    = ntohl(*(uint32_t *)(p_request_packet + 8u));

    uint16_t username_size = p_session->username_size;
    uint16_t password_size = p_session->password_size;

    p_request->size += username_size + password_size;

    p_response->size = 6u;

    if (p_request->size > max_packet_size)
    {
        p_response->retcode = RETCODE_FAILURE;
        fprintf(stderr, "Login request size exceeds max_packet_size\n");
        sockutil_drain(sockfd, p_request->size - 12u, chunk_size);
        goto cleanup;
    }

    char const * p_username_msg = "Username: 3-16 alphanumeric or underscore";
    char const * p_password_msg = "Password: 8-128 printable characters excluding space";

    // Validate username and password length
    if (!((3u <= username_size) && (16u >= username_size)))
    {
        p_response->retcode = RETCODE_FAILURE;
        fprintf(stderr, "%s\n", p_username_msg);
        sockutil_drain(sockfd, p_request->size - 12u, chunk_size);
        goto cleanup;
    }

    if (!((8u <= password_size) && (128u >= password_size)))
    {
        p_response->retcode = RETCODE_FAILURE;
        fprintf(stderr, "%s\n", p_password_msg);
        sockutil_drain(sockfd, p_request->size - 12u, chunk_size);
        goto cleanup;
    }

    // NOTE: realloc behaves like malloc when pointer argument is NULL

    p_tmp = realloc(p_session->p_username, username_size);
    if (NULL == p_tmp)
    {
        p_response->retcode = RETCODE_FAILURE;
        fprintf(stderr, "realloc failed in opcode_login\n");
        sockutil_drain(sockfd, p_request->size - 12u, chunk_size);
        goto cleanup;
    }
    p_session->p_username = p_tmp;

    p_tmp = realloc(p_session->p_password, password_size);
    if (NULL == p_tmp)
    {
        p_response->retcode = RETCODE_FAILURE;
        fprintf(stderr, "realloc failed in opcode_login\n");
        sockutil_drain(sockfd, p_request->size - 12u, chunk_size);
        goto cleanup;
    }
    p_session->p_password = p_tmp;

    char * p_username = p_session->p_username;
    char * p_password = p_session->p_password;

    sockutil_recvall(sockfd, p_request_packet + 12u, p_request->size - 12u);

    memcpy(p_username, p_request_packet + 12u, username_size);
    memcpy(p_password, p_request_packet + 12u + username_size, password_size);

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

    // Validate username and password content
    for (size_t idx = 0u; idx < username_size; idx++)
    {
        char chr = p_username[idx];
        if (!(isalnum(chr) || ('_' == chr)))
        {
            p_response->retcode = RETCODE_FAILURE;
            fprintf(stderr, "%s\n", p_username_msg);
            goto cleanup;
        }
    }

    for (size_t idx = 0u; idx < password_size; idx++)
    {
        char chr = p_password[idx];
        if (!(isprint(chr) && (' ' != chr)))
        {
            p_response->retcode = RETCODE_FAILURE;
            fprintf(stderr, "%s\n", p_password_msg);
            goto cleanup;
        }
    }

    p_appdata    = p_session->p_server->p_appdata;
    p_cred_store = p_appdata->p_cred_store;

    pthread_mutex_lock(&(p_appdata->lock));

    // Prevent integer overflow from sending session ID zero
    if (0u == p_appdata->next_session_id)
    {
        fprintf(stderr, "Why so many sessions?\n");
        p_appdata->next_session_id = 1u;
    }

    // Authenticate login against hash table
    p_item = ht_get(p_cred_store, p_username, username_size);

    pthread_mutex_unlock(&(p_appdata->lock));

    if (NULL == p_item)
    {
        // NOTE: User doesn't exist

        // Create new user
        pthread_mutex_lock(&(p_appdata->lock));
        status = ht_set(
            p_cred_store,
            p_username,
            username_size,
            p_password,
            password_size
        );
        pthread_mutex_unlock(&(p_appdata->lock));

        if (STATUS_SUCCESS != status)
        {
            fprintf(stderr, "ht_set failed\n");

            // Don't exit early with non-success status so response gets sent
            p_response->retcode = RETCODE_FAILURE;
            status = STATUS_SUCCESS;
            goto cleanup;
        }

        // NOTE: Successful user creation

        p_response->retcode = RETCODE_SUCCESS;

        pthread_mutex_lock(&(p_appdata->lock));
        p_session->session_id = (p_appdata->next_session_id)++;
        pthread_mutex_unlock(&(p_appdata->lock));

        *(uint32_t *)(p_response_packet + 2u) = htonl(p_session->session_id);

        if (p_server->b_verbose)
        {
            printf("Created new user: %.*s\n", username_size, p_username);
        }

        goto cleanup;
    }

    // NOTE: User already exists

    // Check if password doesn't match
    if (!(
        (p_item->value_size == password_size) &&
        (0 == memcmp(p_item->p_value, p_password, password_size))
    ))
    {
        // NOTE: Incorrect password

        p_response->retcode = RETCODE_FAILURE;

        p_session->session_id = 0u;
        *(uint32_t *)(p_response_packet + 2u) = htonl(p_session->session_id);

        if (p_server->b_verbose)
        {
            printf(
                "Incorrect password for user: %.*s\n",
                username_size,
                p_username
            );
        }

        goto cleanup;
    }

    // NOTE: Successful login

    p_response->retcode = RETCODE_SUCCESS;

    pthread_mutex_lock(&(p_appdata->lock));
    p_session->session_id = (p_appdata->next_session_id)++;
    pthread_mutex_unlock(&(p_appdata->lock));

    *(uint32_t *)(p_response_packet + 2u) = htonl(p_session->session_id);

    if (p_server->b_verbose)
    {
        printf("Successful login to user: %.*s\n", username_size, p_username);
    }

cleanup:
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

    int        sockfd            = -1;
    server_t * p_server          = NULL;
    uint8_t  * p_request_packet  = NULL;
    uint8_t  * p_response_packet = NULL;

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

    p_request->size = 6u;
    sockutil_recvall(sockfd, p_request_packet + 1u, p_request->size - 1u);

    p_request->session_id = ntohl(*(uint32_t *)(p_request_packet + 2u));

    p_response->size = 1u;

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

    p_session->username_size = 0u;
    p_session->password_size = 0u;
    p_session->session_id    = 0u;

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

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    ;

cleanup:
    return status;
}

status_t
opcode_msg_recv (
    session_t  * p_session,
    request_t  * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    ;

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

/*** end of file ***/
