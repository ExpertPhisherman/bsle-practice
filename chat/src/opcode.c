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

    if ((NULL == p_session) || (NULL == p_request) || (NULL == p_response))
    {
        goto cleanup;
    }

    uint32_t host_request_size = ntohl(p_request->size);

    p_session->username_size = (p_request->p_payload)[0];
    p_session->password_size = (p_request->p_payload)[1];

    if (2u + p_session->username_size + p_session->password_size !=
        host_request_size)
    {
        fprintf(stderr, "Rejecting malformed size in login request\n");
        status = STATUS_SIZE_MISMATCH;
        goto cleanup;
    }

    p_session->p_username = malloc(p_session->username_size);
    if (NULL == p_session->p_username)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_session->p_password = malloc(p_session->password_size);
    if (NULL == p_session->p_password)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memcpy(
        p_session->p_username,
        p_request->p_payload + 2,
        p_session->username_size
    );
    memcpy(
        p_session->p_password,
        p_request->p_payload + 2 + p_session->username_size,
        p_session->password_size
    );

    // TODO: Validate login against hash table
    printf("Username: %.*s\n", p_session->username_size, p_session->p_username);
    printf("Password: %.*s\n", p_session->password_size, p_session->p_password);

    // TODO: Set random non-negative session ID
    p_session->session_id = 1234u;

cleanup:
    if (STATUS_SUCCESS != status)
    {
        opcode_logout(p_session, p_request, p_response);
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
