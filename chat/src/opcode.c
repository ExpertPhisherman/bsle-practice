/** @file opcode.c
 *
 * @brief Opcode source
 *
 * @par
 *
 */

#include "opcode.h"

extern uint32_t const max_payload_size;

status_t
write_response (
    response_t * p_response,
    size_t size,
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

    va_list args;
    va_start(args, p_fmt);
    int written = vsnprintf(p_response->p_payload, size, p_fmt, args);
    va_end(args);

    if (0 > written)
    {
        status = STATUS_FAILURE;
        goto cleanup;
    }

    p_response->size = htonl((uint32_t)written);

cleanup:
    return status;
}

status_t
opcode_default (
    session_t * p_session,
    request_t * p_request,
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
        max_payload_size,
        "Unknown opcode: 0x%02hhx",
        p_request->opcode
    );

cleanup:
    return status;
}

status_t
opcode_ping (
    session_t * p_session,
    request_t * p_request,
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
    status = write_response(p_response, max_payload_size, "PONG");

cleanup:
    return status;
}

status_t
opcode_echo (
    session_t * p_session,
    request_t * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    uint32_t host_request_size = ntohl(p_request->size);

    p_response->status = 0x00;
    status = write_response(
        p_response,
        max_payload_size,
        "%.*s",
        host_request_size,
        p_request->p_payload
    );

cleanup:
    return status;
}

status_t
opcode_quit (
    session_t * p_session,
    request_t * p_request,
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
    status = write_response(p_response, max_payload_size, "Goodbye!");

cleanup:
    return status;
}

status_t
opcode_login (
    session_t * p_session,
    request_t * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;

    appdata_t * p_appdata = NULL;
    safe_ht_t * p_safe_ht = NULL;
    item_t    * p_item    = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

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

    if (2u + username_size + password_size !=
        host_request_size)
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

    /*
    Validate username and password length and content
    Username: 3-16 alphanumeric or underscore characters
    Password: 8+ ASCII characters excluding space
    */
    char const * p_username_msg = "Username must be 3-16 alphanumeric or underscore characters";
    char const * p_password_msg = "Password must be 8+ ASCII characters excluding space";

    if (!((3u <= username_size) && (16u >= username_size)))
    {
        p_response->status = 0x01;
        status = write_response(p_response, max_payload_size, p_username_msg);
        goto cleanup;
    }

    if (!(8u <= password_size))
    {
        p_response->status = 0x01;
        status = write_response(p_response, max_payload_size, p_password_msg);
        goto cleanup;
    }

    for (size_t idx = 0u; idx < username_size; idx++)
    {
        char chr = p_username[idx];
        if (!(isalnum(chr) || ('_' == chr)))
        {
            p_response->status = 0x01;
            status = write_response(p_response, max_payload_size, p_username_msg);
            goto cleanup;
        }
    }

    for (size_t idx = 0u; idx < password_size; idx++)
    {
        char chr = p_password[idx];
        if (!(isprint(chr) && (' ' != chr)))
        {
            p_response->status = 0x01;
            status = write_response(p_response, max_payload_size, p_password_msg);
            goto cleanup;
        }
    }

    p_appdata = p_session->p_server->p_appdata;
    p_safe_ht = p_appdata->p_safe_ht;

    // Authenticate login against hash table
    p_item = MUTEX_CALL(ht_get, p_safe_ht->lock, p_safe_ht->p_ht, p_username, username_size);
    if (NULL == p_item)
    {
        // NOTE: User doesn't exist
        // Create new user
        MUTEX_CALL(
            ht_set,
            p_safe_ht->lock,
            p_safe_ht->p_ht,
            p_username,
            username_size,
            p_password,
            password_size
        );

        p_response->status = 0x00;
        status = write_response(
            p_response,
            max_payload_size,
            "Created new user: %.*s",
            username_size,
            p_username
        );
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }
    }
    else
    {
        // NOTE: User already exists
        MUTEX_CALL(p_safe_ht->p_ht->p_display_item, p_safe_ht->lock, p_item);
        printf("\n");

        // Check if password doesn't match
        if (!(
            (p_item->value_size == password_size) &&
            (0 == memcmp(p_item->p_value, p_password, password_size))
        ))
        {
            p_response->status = 0x01;
            status = write_response(p_response, max_payload_size, "GET OUT!!1!1!");
            goto cleanup;
        }

        p_response->status = 0x00;
        status = write_response(
            p_response,
            max_payload_size,
            "Successful login from user: %.*s",
            username_size,
            p_username
        );
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }
    }

    // NOTE: Successful login
    // TODO: Set random positive session ID
    p_session->session_id = 1234u;

cleanup:
    if (STATUS_SUCCESS != status)
    {
        p_response->status = 0x01;
        write_response(
            p_response,
            max_payload_size,
            "Unexpected error encountered"
        );
        opcode_logout(p_session, p_request, p_response);
    }

    return status;
}

status_t
opcode_logout (
    session_t * p_session,
    request_t * p_request,
    response_t * p_response
)
{
    status_t status = STATUS_SUCCESS;
    UNUSED(p_request);
    UNUSED(p_response);

    if (NULL == p_session)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // TODO: Send logout message

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
