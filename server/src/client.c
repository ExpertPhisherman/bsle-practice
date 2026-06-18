/** @file client.c
 *
 * @brief Generic TCP client source
 *
 * @par
 *
 */

#include "client.h"

extern _Atomic bool gb_running;

void
client_run_wrapper (void * p_arg)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_arg)
    {
        goto cleanup;
    }

    server_client_pair_t * p_pair   = p_arg;
    server_t             * p_server = p_pair->p_server;
    client_t             * p_client = p_pair->p_client;

    free(p_pair);
    p_pair = NULL;

    if ((NULL == p_server) || (NULL == p_client))
    {
        goto cleanup;
    }

    if (NULL == p_server->p_client_run)
    {
        fprintf(stderr, "App not loaded\n");
        client_destroy(p_server, p_client);
        goto cleanup;
    }

    status = (p_server->p_client_run)(p_server, p_client);

    if (STATUS_SUCCESS != status)
    {
        if ((STATUS_CLIENT_DISCONNECT == status) && (p_server->b_verbose))
        {
            printf(
                "Graceful disconnect from client %s:%hu (sockfd %d)\n",
                p_client->p_rhost,
                p_client->rport,
                p_client->sockfd
            );
        }

        client_destroy(p_server, p_client);
        goto cleanup;
    }

    // Re-arm so the next request triggers a new dispatch
    struct epoll_event client_ev = {0};

    client_ev.events   = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP;
    client_ev.data.ptr = p_client;

    if (-1 == epoll_ctl(
        p_server->epollfd,
        EPOLL_CTL_MOD,
        p_client->sockfd,
        &client_ev
    ))
    {
        perror("epoll_ctl MOD re-arm");
        client_destroy(p_server, p_client);
    }

cleanup:
    return;
}

client_t *
client_create (server_t * p_server)
{
    status_t   status   = STATUS_SUCCESS;
    client_t * p_client = NULL;
    char     * p_rhost  = NULL;

    if (NULL == p_server)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_client = calloc(1u, sizeof(*p_client));
    if (NULL == p_client)
    {
        fprintf(stderr, "calloc failed in client_create\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Initialise to safe sentinel values
    p_client->rport            = 0u;
    p_client->p_rhost          = NULL;
    p_client->sockfd           = -1;
    p_client->registry_idx     = -1;
    p_client->p_clientdata     = NULL;

    if (0 != pthread_mutex_init(&(p_client->lock), NULL))
    {
        perror("pthread_mutex_init");
        status = STATUS_MUTEX_FAILURE;
        goto cleanup;
    }

    struct sockaddr_in client_addr = {0};

    socklen_t sin_size = sizeof(client_addr);

    int sockfd = accept(
        p_server->sockfd,
        (struct sockaddr *)&client_addr,
        &sin_size
    );

    if (-1 == sockfd)
    {
        if (EINTR == errno)
        {
            status = gb_running ?
                STATUS_SOCKET_FAILURE :
                STATUS_SERVER_DISCONNECT;
            goto cleanup;
        }

        perror("accept");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }
    p_client->sockfd = sockfd;

    uint16_t rport = ntohs(client_addr.sin_port);
    p_client->rport = rport;

    p_rhost = calloc(1u, INET_ADDRSTRLEN);
    if (NULL == p_rhost)
    {
        fprintf(stderr, "calloc failed in client_create\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_client->p_rhost = p_rhost;

    inet_ntop(
        AF_INET,
        &(client_addr.sin_addr.s_addr),
        p_rhost,
        INET_ADDRSTRLEN
    );

    if (p_server->b_verbose)
    {
        printf(
            "Accepted connection from client %s:%hu (sockfd %d)\n",
            p_rhost,
            rport,
            sockfd
        );
    }

    status = registry_add(p_server->p_registry, p_client);
    if (STATUS_SUCCESS != status)
    {
        goto cleanup;
    }

    // Allow the application layer to allocate per-client state
    if (NULL != p_server->p_client_init)
    {
        status = (p_server->p_client_init)(p_server, p_client);
        if (STATUS_SUCCESS != status)
        {
            fprintf(stderr, "p_client_init failed\n");
            goto cleanup;
        }
    }

cleanup:
    if (STATUS_SUCCESS != status)
    {
        client_destroy(p_server, p_client);
        p_client = NULL;
    }
    return p_client;
}

status_t
client_destroy (server_t * p_server, client_t * p_client)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_server) || (NULL == p_client))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Allow the application layer to free per-client state
    if (NULL != p_server->p_client_free)
    {
        (p_server->p_client_free)(p_server, p_client);
        p_client->p_clientdata = NULL;
    }

    registry_remove(p_server->p_registry, p_client);

    pthread_mutex_destroy(&(p_client->lock));

    free(p_client->p_rhost);
    p_client->p_rhost = NULL;

    if (-1 == p_client->sockfd)
    {
        goto cleanup;
    }

    if (-1 == epoll_ctl(
        p_server->epollfd,
        EPOLL_CTL_DEL,
        p_client->sockfd,
        NULL
    ))
    {
        if (ENOENT != errno)
        {
            perror("epoll_ctl DEL client sockfd");
        }
    }

    if (-1 == shutdown(p_client->sockfd, SHUT_RDWR))
    {
        if (ENOTCONN != errno)
        {
            perror("shutdown client sockfd");
        }
    }

    if (-1 == close(p_client->sockfd))
    {
        perror("close client sockfd");
    }
    p_client->sockfd = -1;

cleanup:
    free(p_client);
    p_client = NULL;
    return status;
}

/*** end of file ***/
