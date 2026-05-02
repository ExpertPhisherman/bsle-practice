/** @file opcode.c
 *
 * @brief Opcode source
 *
 * @par
 *
 */

#include "opcode.h"

status_t
opcode_login (session_t * p_session,
              request_t * p_request,
              response_t * p_response)
{
    status_t status = STATUS_SUCCESS;

    appdata_t * p_appdata = NULL;
    safe_ht_t * p_safe_ht = NULL;
    item_t * p_item       = NULL;

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    char const * p_response_payload = "";
    uint32_t host_response_size = 0u;

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
        p_response_payload = p_username_msg;
        host_response_size = 59u;
        goto cleanup;
    }

    if (!(8u <= password_size))
    {
        p_response_payload = p_password_msg;
        host_response_size = 52u;
        goto cleanup;
    }

    for (size_t idx = 0u; idx < username_size; idx++)
    {
        char chr = p_username[idx];
        if (!(isalnum(chr) || ('_' == chr)))
        {
            p_response_payload = p_username_msg;
            host_response_size = 59u;
            goto cleanup;
        }
    }

    for (size_t idx = 0u; idx < password_size; idx++)
    {
        char chr = p_password[idx];
        if (!(isascii(chr) && (' ' != chr)))
        {
            p_response_payload = p_password_msg;
            host_response_size = 52u;
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

        p_response_payload = "Created new user";
        host_response_size = 16u;
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
            p_response_payload = "GET OUT!!1!1!";
            host_response_size = 13u;
            goto cleanup;
        }

        p_response_payload = "Successful login";
        host_response_size = 16u;
    }

    // TODO: Set random positive session ID
    p_session->session_id = 1234u;

cleanup:
    if (STATUS_SUCCESS != status)
    {
        opcode_logout(p_session, p_request, p_response);
    }
    else
    {
        p_response->size = htonl(host_response_size);
        memcpy(
            p_response->p_payload,
            p_response_payload,
            host_response_size
        );
    }

    return status;
}

status_t
opcode_logout (session_t * p_session,
               request_t * p_request,
               response_t * p_response)
{
    status_t status = STATUS_SUCCESS;
    UNUSED(p_request);
    UNUSED(p_response);

    if (NULL == p_session)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    free(p_session->p_username);
    p_session->p_username = NULL;
    free(p_session->p_password);
    p_session->p_password = NULL;

    p_session->username_size = 0u;
    p_session->password_size = 0u;
    p_session->session_id = 0u;

cleanup:
    return status;
}

/*** end of file ***/
