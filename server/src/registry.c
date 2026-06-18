/** @file registry.c
 *
 * @brief Generic TCP client registry source
 *
 * @par
 *
 */

#include "registry.h"

extern uint32_t const g_max_clients;

registry_t *
registry_create (void)
{
    status_t status = STATUS_SUCCESS;

    registry_t * p_registry = calloc(1u, sizeof(*p_registry));
    if (NULL == p_registry)
    {
        fprintf(stderr, "calloc failed in registry_create\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_registry->pp_clients = calloc(
        g_max_clients,
        sizeof(*(p_registry->pp_clients))
    );
    if (NULL == p_registry->pp_clients)
    {
        fprintf(stderr, "calloc failed in registry_create\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    if (0 != pthread_mutex_init(&(p_registry->lock), NULL))
    {
        perror("pthread_mutex_init");
        status = STATUS_MUTEX_FAILURE;
        goto cleanup;
    }

cleanup:
    if (STATUS_SUCCESS != status)
    {
        registry_destroy(p_registry);
        p_registry = NULL;
    }
    return p_registry;
}

status_t
registry_add (registry_t * p_registry, client_t * p_client)
{
    status_t status = STATUS_FULL;

    if ((NULL == p_registry) || (NULL == p_client))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (-1 == p_client->sockfd)
    {
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    pthread_mutex_lock(&(p_registry->lock));
    for (size_t idx = 0u; idx < g_max_clients; idx++)
    {
        if (NULL == (p_registry->pp_clients)[idx])
        {
            (p_registry->pp_clients)[idx] = p_client;
            p_client->registry_idx = idx;
            status = STATUS_SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&(p_registry->lock));

    if (STATUS_FULL == status)
    {
        fprintf(
            stderr,
            "Registry is full, rejecting client %s:%hu (sockfd %d)\n",
            (NULL != p_client->p_rhost) ? p_client->p_rhost : "<unknown>",
            p_client->rport,
            p_client->sockfd
        );
    }

cleanup:
    return status;
}

status_t
registry_remove (registry_t * p_registry, client_t * p_client)
{
    status_t status = STATUS_NOT_EXISTS;

    if ((NULL == p_registry) || (NULL == p_client))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (-1 == p_client->sockfd)
    {
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    pthread_mutex_lock(&(p_registry->lock));
    if (-1 != p_client->registry_idx)
    {
        (p_registry->pp_clients)[p_client->registry_idx] = NULL;
        p_client->registry_idx = -1;
        status = STATUS_SUCCESS;
    }
    pthread_mutex_unlock(&(p_registry->lock));

    if (STATUS_NOT_EXISTS == status)
    {
        fprintf(
            stderr,
            "Registry does not contain client %s:%hu (sockfd %d)\n",
            (NULL != p_client->p_rhost) ? p_client->p_rhost : "<unknown>",
            p_client->rport,
            p_client->sockfd
        );
    }

cleanup:
    return status;
}

status_t
registry_shutdown (registry_t * p_registry)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_registry)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Unblock all workers blocked in recv()
    pthread_mutex_lock(&(p_registry->lock));
    for (size_t idx = 0u; idx < g_max_clients; idx++)
    {
        client_t * p_client = (p_registry->pp_clients)[idx];
        if (NULL == p_client)
        {
            continue;
        }

        if (-1 == shutdown(p_client->sockfd, SHUT_RDWR))
        {
            if (ENOTCONN != errno)
            {
                perror("shutdown in registry_shutdown");
            }
        }
    }
    pthread_mutex_unlock(&(p_registry->lock));

cleanup:
    return status;
}

status_t
registry_destroy (registry_t * p_registry)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_registry)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // NOTE: All clients must already have been destroyed
    free(p_registry->pp_clients);
    p_registry->pp_clients = NULL;

    pthread_mutex_destroy(&(p_registry->lock));

    free(p_registry);
    p_registry = NULL;

cleanup:
    return status;
}

/*** end of file ***/
