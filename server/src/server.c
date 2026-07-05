/** @file server.c
 *
 * @brief Generic TCP server source
 *
 * @par
 *
 */

#include "server.h"
#include "client.h"
#include "registry.h"

_Atomic bool gb_running = true;

/*!
 * @brief Gracefully shutdown server on SIGINT
 *
 * @param[in] signo Signal number
 *
 * @return void
 */
static void handle_sigint(int signo);

server_t *
server_create (server_t * p_hints)
{
    status_t status = STATUS_SUCCESS;

    server_t * p_server = NULL;

    if (NULL == p_hints)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    struct sigaction sa_int = {0};

    sa_int.sa_handler = handle_sigint;
    sa_int.sa_flags   = 0;
    sigemptyset(&(sa_int.sa_mask));

    if (-1 == sigaction(SIGINT, &sa_int, NULL))
    {
        perror("sigaction SIGINT");
        status = STATUS_SIGNAL_FAILURE;
        goto cleanup;
    }

    p_server = calloc(1u, sizeof(*p_server));
    if (NULL == p_server)
    {
        fprintf(stderr, "calloc failed in server_create\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    *p_server = *p_hints;

    // Catch privileged port as non-root user
    if (
        (PRIVILEGED_PORT_MIN <= p_server->lport) &&
        (PRIVILEGED_PORT_MAX >= p_server->lport) &&
        (0 != geteuid())
    )
    {
        fprintf(
            stderr,
            "Cannot bind to privileged port %hu [%hu-%hu] as non-root user\n",
            p_server->lport,
            PRIVILEGED_PORT_MIN,
            PRIVILEGED_PORT_MAX
        );
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (NULL != p_server->p_server_init)
    {
        status = (p_server->p_server_init)(p_server);
        if (STATUS_SUCCESS != status)
        {
            fprintf(stderr, "p_server_init failed\n");
            goto cleanup;
        }
    }

    p_server->p_lhost    = NULL;
    p_server->sockfd     = -1;
    p_server->epollfd    = -1;
    p_server->p_tm       = NULL;
    p_server->p_registry = NULL;

    p_server->p_registry = registry_create();
    if (NULL == p_server->p_registry)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_server->p_tm = tpool_create(WORKER_THREADS);
    if (NULL == p_server->p_tm)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    struct sockaddr_in server_addr = {0};
    socklen_t sin_size             = sizeof(server_addr);
    int const yes                  = 1;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        perror("socket");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }
    p_server->sockfd = sockfd;

    if (-1 == setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
    {
        perror("setsockopt");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(p_server->lport);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (-1 == bind(sockfd, (struct sockaddr *)&server_addr, sin_size))
    {
        perror("bind");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    // Overwrite local port in case of bind on port 0
    getsockname(sockfd, (struct sockaddr *)&server_addr, &sin_size);
    p_server->lport = ntohs(server_addr.sin_port);

    if (-1 == listen(sockfd, SOMAXCONN))
    {
        perror("listen");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    int epollfd = epoll_create1(0);
    if (-1 == epollfd)
    {
        perror("epoll_create1");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }
    p_server->epollfd = epollfd;

    // Add server socket to epoll
    struct epoll_event server_ev = {0};

    server_ev.events   = EPOLLIN;
    server_ev.data.ptr = p_server;

    if (-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &server_ev))
    {
        perror("epoll_ctl ADD server sockfd");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    char * p_lhost = calloc(INET_ADDRSTRLEN, sizeof(*p_lhost));
    if (NULL == p_lhost)
    {
        fprintf(stderr, "calloc failed in server_create\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_server->p_lhost = p_lhost;

    inet_ntop(
        AF_INET,
        &(server_addr.sin_addr.s_addr),
        p_lhost,
        INET_ADDRSTRLEN
    );

    printf(
        "Listening on server %s:%hu (sockfd %d)\n",
        p_lhost,
        p_server->lport,
        sockfd
    );

cleanup:
    if (STATUS_SUCCESS != status)
    {
        server_destroy(p_server);
        p_server = NULL;
    }
    return p_server;
}

status_t
server_run (server_t * p_server)
{
    status_t status = STATUS_SUCCESS;

    struct epoll_event * p_events = NULL;

    if (NULL == p_server)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_events = calloc(EPOLL_MAX_EVENTS, sizeof(*p_events));
    if (NULL == p_events)
    {
        fprintf(stderr, "calloc failed in server_run\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    while (gb_running)
    {
        int nfds = epoll_wait(
            p_server->epollfd,
            p_events,
            EPOLL_MAX_EVENTS,
            -1
        );

        if (-1 == nfds)
        {
            if (EINTR == errno)
            {
                continue;
            }

            perror("epoll_wait");
            status = STATUS_SOCKET_FAILURE;
            goto cleanup;
        }

        for (size_t idx = 0u; idx < (size_t)nfds; idx++)
        {
            struct epoll_event event = p_events[idx];

            if (event.data.ptr == p_server)
            {
                // NOTE: Server socket is readable
                // Accept new client
                client_t * p_client = client_create(p_server);
                if (NULL == p_client)
                {
                    fprintf(stderr, "client_create failed\n");
                    continue;
                }

                struct epoll_event client_ev = {0};

                client_ev.events   = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP;
                client_ev.data.ptr = p_client;

                if (-1 == epoll_ctl(
                    p_server->epollfd,
                    EPOLL_CTL_ADD,
                    p_client->sockfd,
                    &client_ev
                ))
                {
                    perror("epoll_ctl ADD client sockfd");
                    client_destroy(p_client);
                    p_client = NULL;
                }

                continue;
            }

            // NOTE: Client socket is readable
            client_t * p_client = event.data.ptr;

            uint32_t const err_events = EPOLLHUP | EPOLLERR | EPOLLRDHUP;

            if (0u != (event.events & err_events))
            {
                fprintf(
                    stderr,
                    "Abrupt disconnect from client %s:%hu (sockfd %d)\n",
                    p_client->p_rhost,
                    p_client->rport,
                    p_client->sockfd
                );

                client_destroy(p_client);
                p_client = NULL;
                continue;
            }

            if (!tpool_add_work(p_server->p_tm, client_run_wrapper, p_client))
            {
                fprintf(stderr, "tpool_add_work failed\n");

                // Re-arm so client is not permanently silenced
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
                    perror("epoll_ctl MOD re-arm after failed tpool_add_work");
                    client_destroy(p_client);
                }
            }

            // NOTE: Ownership of p_client transferred to worker thread
            p_client = NULL;
        }
    }

    printf(
        "\nGraceful shutdown on server %s:%hu (sockfd %d)\n",
        p_server->p_lhost,
        p_server->lport,
        p_server->sockfd
    );

cleanup:
    free(p_events);
    p_events = NULL;

    return status;
}

status_t
server_destroy (server_t * p_server)
{
    status_t status = STATUS_SUCCESS;

    if (NULL == p_server)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    registry_shutdown(p_server->p_registry);

    // Wait for workers to finish and join all threads
    tpool_wait(p_server->p_tm);

    tpool_destroy(p_server->p_tm);
    p_server->p_tm = NULL;

    registry_destroy(p_server->p_registry);
    p_server->p_registry = NULL;

    if (NULL != p_server->p_server_free)
    {
        (p_server->p_server_free)(p_server);
    }

    if ((-1 != p_server->epollfd) && (-1 == close(p_server->epollfd)))
    {
        perror("close epollfd");
        status = STATUS_SOCKET_FAILURE;
    }
    p_server->epollfd = -1;

    if ((-1 != p_server->sockfd) && (-1 == close(p_server->sockfd)))
    {
        perror("close sockfd");
        status = STATUS_SOCKET_FAILURE;
    }
    p_server->sockfd = -1;

    free(p_server->p_lhost);
    p_server->p_lhost   = NULL;
    p_server->p_appdata = NULL;

cleanup:
    free(p_server);
    p_server = NULL;
    return status;
}

static void
handle_sigint (int signo)
{
    UNUSED(signo);
    gb_running = false;
    return;
}

/*** end of file ***/
