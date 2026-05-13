/** @file chat_opcode.c
 *
 * @brief Chat opcode source
 *
 * @par
 *
 */

#include "chat_opcode.h"

extern uint32_t const max_payload_size;

status_t
write_response (
    response_t * p_response,
    char const * p_fmt,
    ...
)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_response) || (NULL == p_fmt))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    va_list p_args;
    va_start(p_args, p_fmt);

    int written = vsnprintf(
        p_response->p_payload,
        max_payload_size,
        p_fmt,
        p_args
    );

    va_end(p_args);

    if (0 > written)
    {
        fprintf(stderr, "write_response: vsnprintf output failure\n");
        status = STATUS_FAILURE;
        goto cleanup;
    }

    p_response->size = htonl((uint32_t)written);

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
    UNUSED(p_request);

    if ((NULL == p_session) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->status = 0x01;

    status = write_response(
        p_response,
        "Unknown opcode: 0x%02hhx",
        p_request->opcode
    );

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
    UNUSED(p_request);

    if ((NULL == p_session) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->status = 0x00;

    status = write_response(p_response, "PONG");

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

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_response->status = 0x00;

    status = write_response(
        p_response,
        "%.*s",
        ntohl(p_request->size),
        p_request->p_payload
    );

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
    UNUSED(p_request);

    if ((NULL == p_session) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    opcode_logout(p_session, p_request, p_response);

    p_response->status = 0x00;

    status = write_response(p_response, "Goodbye!");

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

    appdata_t * p_appdata    = NULL;
    ht_t      * p_cred_store = NULL;
    uint32_t  * p_session_id = NULL;
    item_t    * p_item       = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Free any existing session credentials before re-allocating
    opcode_logout(p_session, p_request, p_response);

    server_t * p_server = p_session->p_server;
    if (NULL == p_server)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    uint32_t host_request_size = ntohl(p_request->size);

    p_session->username_size = (p_request->p_payload)[0];
    p_session->password_size = (p_request->p_payload)[1];

    uint8_t username_size = p_session->username_size;
    uint8_t password_size = p_session->password_size;

    if (2u + username_size + password_size != host_request_size)
    {
        fprintf(stderr, "Rejecting malformed size in login request\n");
        status = STATUS_SIZE_MISMATCH;
        goto cleanup;
    }

    p_session->p_username = malloc(username_size);
    if (NULL == p_session->p_username)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_session->p_password = malloc(password_size);
    if (NULL == p_session->p_password)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    char * p_username = p_session->p_username;
    char * p_password = p_session->p_password;

    // Write username and password into session
    memcpy(
        p_username,
        p_request->p_payload + 2,
        username_size
    );
    memcpy(
        p_password,
        p_request->p_payload + 2 + username_size,
        password_size
    );

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

    // Validate username and password length and content
    char const * p_username_msg = "Username: 3-16 alphanumeric or underscore characters";
    char const * p_password_msg = "Password: 8+ ASCII characters excluding space";

    if (!((3u <= username_size) && (16u >= username_size)))
    {
        p_response->status = 0x01;

        status = write_response(p_response, p_username_msg);
        goto cleanup;
    }

    if (!(8u <= password_size))
    {
        p_response->status = 0x01;

        status = write_response(p_response, p_password_msg);
        goto cleanup;
    }

    for (size_t idx = 0u; idx < username_size; idx++)
    {
        char chr = p_username[idx];
        if (!(isalnum(chr) || ('_' == chr)))
        {
            p_response->status = 0x01;

            status = write_response(p_response, p_username_msg);
            goto cleanup;
        }
    }

    for (size_t idx = 0u; idx < password_size; idx++)
    {
        char chr = p_password[idx];
        if (!(isprint(chr) && (' ' != chr)))
        {
            p_response->status = 0x01;

            status = write_response(p_response, p_password_msg);
            goto cleanup;
        }
    }

    p_appdata    = p_session->p_server->p_appdata;
    p_cred_store = p_appdata->p_cred_store;
    p_session_id = p_appdata->p_session_id;

    pthread_mutex_lock(&(p_appdata->lock));

    // Prevent integer overflow from sending session ID zero
    if (0u == *p_session_id)
    {
        *p_session_id = 1u;
    }

    // Authenticate login against hash table
    p_item = ht_get(p_cred_store, p_username, username_size);

    pthread_mutex_unlock(&(p_appdata->lock));

    if (NULL == p_item)
    {
        // NOTE: User doesn't exist
        // Create new user
        pthread_mutex_lock(&(p_appdata->lock));
        ht_set(
            p_cred_store,
            p_username,
            username_size,
            p_password,
            password_size
        );
        pthread_mutex_unlock(&(p_appdata->lock));

        p_response->status = 0x00;

        // NOTE: Successful user creation
        // TODO: Set unique positive session ID
        p_session->session_id = 255u;

        uint32_t net_session_id = htonl(p_session->session_id);

        status = write_response(
            p_response,
            "%c%c%c%cCreated new user: %.*s",
            (net_session_id >>  0) & 0xFF,
            (net_session_id >>  8) & 0xFF,
            (net_session_id >> 16) & 0xFF,
            (net_session_id >> 24) & 0xFF,
            username_size,
            p_username
        );
    }
    else
    {
        // NOTE: User already exists

        // Check if password doesn't match
        if (!(
            (p_item->value_size == password_size) &&
            (0 == memcmp(p_item->p_value, p_password, password_size))
        ))
        {
            p_response->status = 0x01;

            p_session->session_id = 0u;

            uint32_t net_session_id = htonl(p_session->session_id);

            status = write_response(
                p_response,
                "%c%c%c%cGET OUT!!1!1!",
                (net_session_id >>  0) & 0xFF,
                (net_session_id >>  8) & 0xFF,
                (net_session_id >> 16) & 0xFF,
                (net_session_id >> 24) & 0xFF
            );
            goto cleanup;
        }

        p_response->status = 0x00;

        // NOTE: Successful login
        // TODO: Set unique positive session ID
        p_session->session_id = 255u;

        uint32_t net_session_id = htonl(p_session->session_id);

        status = write_response(
            p_response,
            "%c%c%c%cSuccessful login to user: %.*s",
            (net_session_id >>  0) & 0xFF,
            (net_session_id >>  8) & 0xFF,
            (net_session_id >> 16) & 0xFF,
            (net_session_id >> 24) & 0xFF,
            username_size,
            p_username
        );
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }
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
    UNUSED(p_request);

    if (NULL == p_session)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (NULL != p_response)
    {
        p_response->status = 0x00;

        status = write_response(
            p_response,
            "Successful logout from user: %.*s",
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
    p_session->session_id    = 0u;

cleanup:
    return status;
}

/*** end of file ***/
