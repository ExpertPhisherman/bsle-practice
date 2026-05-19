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

    int        sockfd   = -1;
    server_t * p_server = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd   = p_session->sockfd;
    p_server = p_session->p_server;

    if (NULL == p_server)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

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

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_request_packet = p_request->p_packet;

    if (NULL == p_request_packet)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_PING;
    p_response->retcode = RETCODE_SUCCESS;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_PADDING
    );

    p_request->size += FIELD_SIZE_PADDING;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_SESSION_ID
    );

    p_request->session_id = ntohl(
        *(uint32_t *)(p_request_packet + p_request->size)
    );

    p_request->size += FIELD_SIZE_SESSION_ID;

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

    p_response->opcode  = OPCODE_ECHO;
    p_response->retcode = RETCODE_SUCCESS;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_PADDING
    );

    p_request->size += FIELD_SIZE_PADDING;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_SIZE
    );

    uint16_t payload_size = ntohs(
        *(uint16_t *)(p_request_packet + p_request->size)
    );

    // Copy size into response
    memcpy(
        p_response_packet + p_response->size,
        p_request_packet + p_request->size,
        FIELD_SIZE_SIZE
    );

    p_request->size += FIELD_SIZE_SIZE;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_SESSION_ID
    );

    p_request->session_id = ntohl(
        *(uint32_t *)(p_request_packet + p_request->size)
    );

    p_request->size += FIELD_SIZE_SESSION_ID;

    if ((p_request->size + payload_size) > max_packet_size)
    {
        // Zero out response size field
        p_response->retcode = RETCODE_OVERFLOW;
        fprintf(stderr, "Echo request size exceeds max_packet_size\n");
        memset(p_response_packet + p_response->size, 0, FIELD_SIZE_SIZE);
        sockutil_drain(sockfd, payload_size, chunk_size);
        payload_size = 0u;
    }

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        // Zero out response size field
        status = STATUS_SUCCESS;
        memset(p_response_packet + p_response->size, 0, FIELD_SIZE_SIZE);
        sockutil_drain(sockfd, payload_size, chunk_size);
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

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_server          = p_session->p_server;
    p_request_packet  = p_request->p_packet;

    if ((NULL == p_server) || (NULL == p_request_packet))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_QUIT;
    p_response->retcode = RETCODE_SUCCESS;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_PADDING
    );

    p_request->size += FIELD_SIZE_PADDING;

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
    ht_t      * p_cred_store      = NULL;
    item_t    * p_item            = NULL;
    int         sockfd            = -1;
    server_t  * p_server          = NULL;
    uint8_t   * p_request_packet  = NULL;
    uint8_t   * p_response_packet = NULL;
    char      * p_tmp             = NULL;
    bool        b_locked          = false;

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

    p_appdata = p_server->p_appdata;
    if (NULL == p_appdata)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_cred_store = p_appdata->p_cred_store;
    if (NULL == p_cred_store)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_LOGIN;
    p_response->retcode = RETCODE_SUCCESS;

    free(p_session->p_username);
    p_session->p_username = NULL;

    free(p_session->p_password);
    p_session->p_password = NULL;

    p_session->username_size = 0u;
    p_session->password_size = 0u;

    p_session->session_id = 0u;

    // TODO: Remove p_session from room's sessions SLL
    ;

    free(p_session->p_room_name);
    p_session->p_room_name = NULL;

    p_session->room_name_size = 0u;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_PADDING * 3
    );

    uint8_t user_flag = p_request_packet[p_request->size];
    UNUSED(user_flag);

    p_request->size += FIELD_SIZE_PADDING * 3;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_SIZE
    );

    p_session->username_size = ntohs(
        *(uint16_t *)(p_request_packet + p_request->size)
    );

    p_request->size += FIELD_SIZE_SIZE;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_SIZE
    );

    p_session->password_size = ntohs(
        *(uint16_t *)(p_request_packet + p_request->size)
    );

    p_request->size += FIELD_SIZE_SIZE;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_SESSION_ID
    );

    p_session->session_id = ntohl(
        *(uint32_t *)(p_request_packet + p_request->size)
    );

    p_request->size += FIELD_SIZE_SESSION_ID;

    uint16_t username_size = p_session->username_size;
    uint16_t password_size = p_session->password_size;

    char const * p_username_msg = "Username: 3-16 alphanumeric or underscore";
    char const * p_password_msg = "Password: 8-128 printable characters excluding space";

    if ((p_request->size + username_size + password_size) > max_packet_size)
    {
        fprintf(stderr, "Login request size exceeds max_packet_size\n");
        sockutil_drain(sockfd, username_size + password_size, chunk_size);

        p_response->retcode   = RETCODE_OVERFLOW;
        p_session->session_id = 0u;
        goto cpy_session_id;
    }

    // Validate username and password length
    if (!((3u <= username_size) && (16u >= username_size)))
    {
        fprintf(stderr, "%s\n", p_username_msg);
        sockutil_drain(sockfd, username_size + password_size, chunk_size);

        p_response->retcode   = RETCODE_FAILURE;
        p_session->session_id = 0u;
        goto cpy_session_id;
    }

    if (!((8u <= password_size) && (128u >= password_size)))
    {
        fprintf(stderr, "%s\n", p_password_msg);
        sockutil_drain(sockfd, username_size + password_size, chunk_size);

        p_response->retcode   = RETCODE_FAILURE;
        p_session->session_id = 0u;
        goto cpy_session_id;
    }

    // NOTE: realloc behaves like malloc when pointer argument is NULL

    p_tmp = realloc(p_session->p_username, username_size);
    if (NULL == p_tmp)
    {
        fprintf(stderr, "realloc failed in opcode_login\n");
        sockutil_drain(sockfd, username_size + password_size, chunk_size);

        p_response->retcode   = RETCODE_FAILURE;
        p_session->session_id = 0u;
        goto cpy_session_id;
    }
    p_session->p_username = p_tmp;

    p_tmp = realloc(p_session->p_password, password_size);
    if (NULL == p_tmp)
    {
        fprintf(stderr, "realloc failed in opcode_login\n");
        sockutil_drain(sockfd, username_size + password_size, chunk_size);

        p_response->retcode   = RETCODE_FAILURE;
        p_session->session_id = 0u;
        goto cpy_session_id;
    }
    p_session->p_password = p_tmp;

    char * p_username = p_session->p_username;
    char * p_password = p_session->p_password;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        username_size
    );

    memcpy(p_username, p_request_packet + p_request->size, username_size);
    p_request->size += username_size;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        password_size
    );

    memcpy(p_password, p_request_packet + p_request->size, password_size);
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

    // Validate username and password content
    for (size_t idx = 0u; idx < username_size; idx++)
    {
        char chr = p_username[idx];
        if (!(isalnum(chr) || ('_' == chr)))
        {
            fprintf(stderr, "%s\n", p_username_msg);

            p_response->retcode   = RETCODE_FAILURE;
            p_session->session_id = 0u;
            goto cpy_session_id;
        }
    }

    for (size_t idx = 0u; idx < password_size; idx++)
    {
        char chr = p_password[idx];
        if (!(isprint(chr) && (' ' != chr)))
        {
            fprintf(stderr, "%s\n", p_password_msg);

            p_response->retcode   = RETCODE_FAILURE;
            p_session->session_id = 0u;
            goto cpy_session_id;
        }
    }

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    // Prevent integer overflow from sending session ID zero
    if (0u == p_appdata->next_session_id)
    {
        fprintf(stderr, "Why so many sessions?\n");
        p_appdata->next_session_id = 1u;
    }

    // Authenticate login against hash table
    p_item = ht_get(p_cred_store, p_username, username_size);

    if (NULL == p_item)
    {
        // NOTE: User doesn't exist

        // Create new user
        status = ht_set(
            p_cred_store,
            p_username,
            username_size,
            p_password,
            password_size
        );

        if (STATUS_SUCCESS != status)
        {
            fprintf(stderr, "ht_set failed\n");
            status = STATUS_SUCCESS;

            p_response->retcode   = RETCODE_FAILURE;
            p_session->session_id = 0u;
            goto cpy_session_id;
        }

        // NOTE: Successful user creation

        p_response->retcode = RETCODE_SUCCESS;

        p_session->session_id = (p_appdata->next_session_id)++;

        if (p_server->b_verbose)
        {
            printf("Created new user: %.*s\n", username_size, p_username);
        }

        goto cpy_session_id;
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

        if (p_server->b_verbose)
        {
            printf(
                "Incorrect password for user: %.*s\n",
                username_size,
                p_username
            );
        }

        goto cpy_session_id;
    }

    // NOTE: Successful login

    p_response->retcode = RETCODE_SUCCESS;

    p_session->session_id = (p_appdata->next_session_id)++;

    if (p_server->b_verbose)
    {
        printf("Successful login to user: %.*s\n", username_size, p_username);
    }

cpy_session_id:
    // Copy session ID into response
    *(uint32_t *)(p_response_packet + p_response->size) = htonl(
        p_session->session_id
    );

    p_response->size += FIELD_SIZE_SESSION_ID;

cleanup:
    // Always set response size so packet sends entirely
    p_response->size = (
        FIELD_SIZE_OPCODE +
        FIELD_SIZE_RETCODE +
        FIELD_SIZE_SESSION_ID
    );

    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
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

    int        sockfd           = -1;
    server_t * p_server         = NULL;
    uint8_t  * p_request_packet = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd           = p_session->sockfd;
    p_server         = p_session->p_server;
    p_request_packet = p_request->p_packet;

    if ((NULL == p_server) || (NULL == p_request_packet))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_LOGOUT;
    p_response->retcode = RETCODE_SUCCESS;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_PADDING
    );

    p_request->size += FIELD_SIZE_PADDING;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_SESSION_ID
    );

    p_request->session_id = ntohl(
        *(uint32_t *)(p_request_packet + p_request->size)
    );

    p_request->size += FIELD_SIZE_SESSION_ID;

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

    free(p_session->p_username);
    p_session->p_username = NULL;

    free(p_session->p_password);
    p_session->p_password = NULL;

    p_session->username_size = 0u;
    p_session->password_size = 0u;

    p_session->session_id = 0u;

    // TODO: Remove p_session from room's sessions SLL
    ;

    free(p_session->p_room_name);
    p_session->p_room_name = NULL;

    p_session->room_name_size = 0u;

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

    appdata_t * p_appdata         = NULL;
    ht_t      * p_room_store      = NULL;
    item_t    * p_item            = NULL;
    node_t    * p_node            = NULL;
    int         sockfd            = -1;
    server_t  * p_server          = NULL;
    uint8_t   * p_request_packet  = NULL;
    room_t    * p_room            = NULL;
    char      * p_message         = NULL;
    bool        b_locked          = false;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_server          = p_session->p_server;
    p_request_packet  = p_request->p_packet;

    if ((NULL == p_server) || (NULL == p_request_packet))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_appdata = p_server->p_appdata;
    if (NULL == p_appdata)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_room_store = p_appdata->p_room_store;
    if (NULL == p_room_store)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_MSG_SEND;
    p_response->retcode = RETCODE_SUCCESS;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_PADDING
    );

    p_request->size += FIELD_SIZE_PADDING;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_SIZE
    );

    uint16_t message_size = ntohs(
        *(uint16_t *)(p_request_packet + p_request->size)
    );

    p_request->size += FIELD_SIZE_SIZE;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_SESSION_ID
    );

    p_request->session_id = ntohl(
        *(uint32_t *)(p_request_packet + p_request->size)
    );

    p_request->size += FIELD_SIZE_SESSION_ID;

    if ((p_request->size + message_size) > max_packet_size)
    {
        p_response->retcode = RETCODE_OVERFLOW;
        fprintf(stderr, "Message send request size exceeds max_packet_size\n");
        sockutil_drain(sockfd, message_size, chunk_size);
        goto cleanup;
    }

    // Receive message
    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        message_size
    );

    status = validate_session(p_session, p_request, p_response);
    if (STATUS_INVALID_SESSION == status)
    {
        status = STATUS_SUCCESS;
        goto cleanup;
    }

    if (NULL == p_session->p_room_name)
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

    p_message = calloc(1u, message_size);
    if (NULL == p_message)
    {
        p_response->retcode = RETCODE_FAILURE;
        fprintf(stderr, "calloc failed in opcode_msg_send\n");
        goto cleanup;
    }

    memcpy(p_message, p_request_packet + p_request->size, message_size);

    p_request->size += message_size;

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    p_item = ht_get(
        p_room_store,
        p_session->p_room_name,
        p_session->room_name_size
    );
    if (NULL == p_item)
    {
        // NOTE: Room doesn't exist

        p_response->retcode = RETCODE_FAILURE;

        printf(
            "Room %.*s doesn't exist\n",
            p_session->room_name_size,
            p_session->p_room_name
        );

        goto cleanup;
    }

    p_room = p_item->p_value;

    p_node = sll_get(p_room->p_sessions, &p_session, sizeof(p_session));
    if (NULL == p_node)
    {
        // NOTE: Session is not member of room

        p_response->retcode = RETCODE_FAILURE;

        printf(
            "Session of user %.*s is not member of room %.*s\n",
            p_session->username_size,
            p_session->p_username,
            p_session->room_name_size,
            p_session->p_room_name
        );

        goto cleanup;
    }

    // TODO: Send message to all sessions in room
    ;

    if (p_server->b_verbose)
    {
        printf(
            "%.*s sent message \"%.*s\" to room: %.*s\n",
            p_session->username_size,
            p_session->p_username,
            message_size,
            p_message,
            p_session->room_name_size,
            p_session->p_room_name
        );
    }

cleanup:
    free(p_message);
    p_message = NULL;

    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
    }
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

    p_response->opcode  = OPCODE_MSG_RECV;
    p_response->retcode = RETCODE_SUCCESS;

    ;

cleanup:
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
    ht_t      * p_room_store      = NULL;
    item_t    * p_item            = NULL;
    int         sockfd            = -1;
    server_t  * p_server          = NULL;
    uint8_t   * p_request_packet  = NULL;
    room_t    * p_room            = NULL;
    void      * p_tmp             = NULL;
    bool        b_locked          = false;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockfd            = p_session->sockfd;
    p_server          = p_session->p_server;
    p_request_packet  = p_request->p_packet;

    if ((NULL == p_server) || (NULL == p_request_packet))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->opcode  = OPCODE_JOIN;
    p_response->retcode = RETCODE_SUCCESS;

    p_appdata = p_server->p_appdata;
    if (NULL == p_appdata)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_room_store = p_appdata->p_room_store;
    if (NULL == p_room_store)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_PADDING
    );

    p_request->size += FIELD_SIZE_PADDING;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_SIZE
    );

    p_session->room_name_size = ntohs(
        *(uint16_t *)(p_request_packet + p_request->size)
    );

    p_request->size += FIELD_SIZE_SIZE;

    sockutil_recvall(
        sockfd,
        p_request_packet + p_request->size,
        FIELD_SIZE_SESSION_ID
    );

    p_request->session_id = ntohl(
        *(uint32_t *)(p_request_packet + p_request->size)
    );

    p_request->size += FIELD_SIZE_SESSION_ID;

    uint16_t room_name_size = p_session->room_name_size;

    if ((p_request->size + room_name_size) > max_packet_size)
    {
        p_response->retcode = RETCODE_OVERFLOW;
        fprintf(stderr, "Join request size exceeds max_packet_size\n");
        sockutil_drain(sockfd, room_name_size, chunk_size);
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

    p_tmp = realloc(p_session->p_room_name, room_name_size);
    if (NULL == p_tmp)
    {
        p_response->retcode = RETCODE_FAILURE;
        fprintf(stderr, "realloc failed in opcode_join\n");
        goto cleanup;
    }
    p_session->p_room_name = p_tmp;

    char * p_room_name = p_session->p_room_name;

    memcpy(p_room_name, p_request_packet + p_request->size, room_name_size);

    p_request->size += room_name_size;

    pthread_mutex_lock(&(p_appdata->lock));
    b_locked = true;

    p_item = ht_get(p_room_store, p_room_name, room_name_size);
    if (NULL == p_item)
    {
        // NOTE: Room doesn't exist

        p_room = room_create(p_room_name, room_name_size);
        if (NULL == p_room)
        {
            status = STATUS_ALLOC_FAILURE;
            fprintf(stderr, "room_create failed\n");
            p_response->retcode = RETCODE_FAILURE;
            goto cleanup;
        }

        status = ht_set(
            p_room_store,
            p_room->p_name,
            p_room->name_size,
            p_room,
            sizeof(*p_room)
        );
        if (STATUS_SUCCESS != status)
        {
            fprintf(stderr, "ht_set failed\n");
            p_response->retcode = RETCODE_FAILURE;

            room_destroy(p_room);
            p_room = NULL;
            goto cleanup;
        }

        // Free room after hash table makes its own copy
        free(p_room);
        p_room = NULL;

        if (p_server->b_verbose)
        {
            printf(
                "%.*s created new room: %.*s\n",
                p_session->username_size,
                p_session->p_username,
                room_name_size,
                p_room_name
            );
        }

        p_item = ht_get(p_room_store, p_room_name, room_name_size);
        if (NULL == p_item)
        {
            fprintf(stderr, "ht_get failed\n");
            p_response->retcode = RETCODE_FAILURE;
            goto cleanup;
        }
    }

    p_room = p_item->p_value;

    status = sll_append(p_room->p_sessions, &p_session, sizeof(p_session));
    if (STATUS_SUCCESS != status)
    {
        fprintf(stderr, "sll_append failed\n");
        p_response->retcode = RETCODE_FAILURE;
        goto cleanup;
    }

    // Join room
    p_session->p_room_name    = p_room_name;
    p_session->room_name_size = room_name_size;

    if (p_server->b_verbose)
    {
        printf(
            "%.*s joined room: %.*s\n",
            p_session->username_size,
            p_session->p_username,
            room_name_size,
            p_room_name
        );
    }

cleanup:
    if (b_locked)
    {
        pthread_mutex_unlock(&(p_appdata->lock));
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
