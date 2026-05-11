/** @file server.c
 *
 * @brief Generic TCP server source
 *
 * @par
 *
 */

#include "server.h"

extern uint32_t const max_clients;
extern uint32_t const worker_threads;
extern uint32_t const epoll_max_events;

uint16_t const max_port = 65535u;

_Atomic sig_atomic_t g_keep_running = 1;

/*!
 * @brief Gracefully shutdown server on SIGINT
 *
 * @param[in] signo Signal number
 *
 * @return void
 */
static void handle_sigint(int signo);

/*!
 * @brief Dispatch single recv/handle/send iteration to the thread pool,
 *        then re-arm client epoll registration
 *
 * @param[in] p_arg Pointer to server_client_pair_t
 *
 * @return void
 */
static void client_run_wrapper(void * p_arg);

/*!
 * @brief Accept new client and initialise per-client state
 *
 * @param[in] p_server Pointer to server
 *
 * @return Pointer to client
 */
static client_t * client_create(server_t * p_server);

/*!
 * @brief Destroy client, remove from epoll, shut down socket, call cleanup
 *
 * @param[in] p_server Pointer to server
 * @param[in] p_client Pointer to client
 *
 * @return Status of operation
 */
static status_t client_destroy(server_t * p_server, client_t * p_client);

/*!
 * @brief Create registry
 *
 * @param[in] void
 *
 * @return Pointer to registry
 */
static registry_t * registry_create(void);

/*!
 * @brief Add client to registry
 *
 * @param[in] p_registry Pointer to registry
 * @param[in] p_client   Pointer to client to add
 *
 * @return Status of operation
 */
static status_t registry_add(registry_t * p_registry, client_t * p_client);

/*!
 * @brief Remove client from registry
 *
 * @param[in] p_registry Pointer to registry
 * @param[in] p_client   Pointer to client to remove
 *
 * @return Status of operation
 */
static status_t registry_remove(registry_t * p_registry, client_t * p_client);

/*!
 * @brief Shutdown all client sockets to unblock workers blocked in recv()
 *
 * @param[in] p_registry Pointer to registry
 *
 * @return Status of operation
 */
static status_t registry_shutdown(registry_t * p_registry);

/*!
 * @brief Destroy clients idle in epoll at shutdown time
 *
 * @param[in] p_server Pointer to server
 *
 * @return Status of operation
 */
static status_t registry_cleanup_clients(server_t * p_server);

/*!
 * @brief Destroy registry
 *
 * @param[in] p_registry Pointer to registry
 *
 * @return Status of operation
 */
static status_t registry_destroy(registry_t * p_registry);

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

    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sa_int.sa_flags   = 0;
    sigemptyset(&sa_int.sa_mask);

    if (-1 == sigaction(SIGINT, &sa_int, NULL))
    {
        perror("sigaction SIGINT");
        status = STATUS_SIGNAL_FAILURE;
        goto cleanup;
    }

    p_server = malloc(sizeof(*p_server));
    if (NULL == p_server)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Initialise to safe sentinel values before copying hints
    p_server->lport         = 0u;
    p_server->p_lhost       = NULL;
    p_server->backlog       = -1;
    p_server->b_verbose     = false;
    p_server->sockfd        = -1;
    p_server->epollfd       = -1;
    p_server->p_tm          = NULL;
    p_server->p_registry    = NULL;
    p_server->p_client_run  = NULL;
    p_server->p_client_init = NULL;
    p_server->p_client_free = NULL;
    p_server->p_appdata     = NULL;

    *p_server = *p_hints;

    // Catch privileged port as non-root user
    if ((1024u > (p_server->lport)) && (0 != geteuid()))
    {
        fprintf(
            stderr,
            "Cannot bind to privileged port %hu [1-1023] as non-root user\n",
            p_server->lport
        );
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    // Reset resource fields that must be created fresh regardless of hints
    p_server->sockfd     = -1;
    p_server->epollfd    = -1;
    p_server->p_tm       = NULL;
    p_server->p_registry = NULL;
    p_server->p_lhost    = NULL;

    p_server->p_registry = registry_create();
    if (NULL == p_server->p_registry)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_server->p_tm = tpool_create(worker_threads);
    if (NULL == p_server->p_tm)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    struct sockaddr_in server_addr;
    int yes = 1;

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

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(p_server->lport);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (-1 == bind(
        sockfd,
        (struct sockaddr *)&server_addr,
        sizeof(server_addr)
    ))
    {
        perror("bind");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (-1 == listen(sockfd, p_server->backlog))
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
    struct epoll_event server_ev;
    memset(&server_ev, 0, sizeof(server_ev));
    server_ev.events   = EPOLLIN;
    server_ev.data.ptr = p_server;

    if (-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &server_ev))
    {
        perror("epoll_ctl ADD server sockfd");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    char * p_lhost = malloc(INET_ADDRSTRLEN);
    if (NULL == p_lhost)
    {
        fprintf(stderr, "malloc failed\n");
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

    if (p_server->b_verbose)
    {
        printf(
            "Listening on server %s:%hu (sockfd %d)\n",
            p_lhost,
            p_server->lport,
            sockfd
        );
    }

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

    p_events = calloc(epoll_max_events, sizeof(*p_events));
    if (NULL == p_events)
    {
        fprintf(stderr, "calloc failed in server_run\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    while (g_keep_running)
    {
        int nfds = epoll_wait(
            p_server->epollfd,
            p_events,
            epoll_max_events,
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
            if (p_events[idx].data.ptr == p_server)
            {
                // NOTE: Server socket is readable

                // Accept new client
                client_t * p_client = client_create(p_server);
                if (NULL == p_client)
                {
                    continue;
                }

                struct epoll_event client_ev;
                memset(&client_ev, 0, sizeof(client_ev));
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
                    client_destroy(p_server, p_client);
                    p_client = NULL;
                }

                continue;
            }

            // NOTE: Client socket is readable
            client_t * p_client = p_events[idx].data.ptr;

            uint32_t const err_events =
                (uint32_t)(EPOLLHUP | EPOLLERR | EPOLLRDHUP);

            if (0u != (p_events[idx].events & err_events))
            {
                fprintf(
                    stderr,
                    "Abrupt disconnect from client %s:%hu (sockfd %d)\n",
                    p_client->p_rhost,
                    p_client->rport,
                    p_client->sockfd
                );
                client_destroy(p_server, p_client);
                p_client = NULL;
                continue;
            }

            server_client_pair_t * p_pair = malloc(sizeof(*p_pair));
            if (NULL == p_pair)
            {
                fprintf(stderr, "malloc failed\n");
                client_destroy(p_server, p_client);
                p_client = NULL;
                continue;
            }

            p_pair->p_server = p_server;
            p_pair->p_client = p_client;

            if (!tpool_add_work(p_server->p_tm, client_run_wrapper, p_pair))
            {
                fprintf(stderr, "tpool_add_work failed\n");
                free(p_pair);
                p_pair = NULL;

                // Re-arm so client is not permanently silenced
                struct epoll_event client_ev;
                memset(&client_ev, 0, sizeof(client_ev));
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
                    client_destroy(p_server, p_client);
                }
            }

            // NOTE: Ownership of p_client transferred to worker thread
            p_client = NULL;
            p_pair   = NULL;
        }
    }

    if (p_server->b_verbose)
    {
        printf(
            "\nGraceful shutdown on server %s:%hu (sockfd %d)\n",
            p_server->p_lhost,
            p_server->lport,
            p_server->sockfd
        );
    }

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

    registry_cleanup_clients(p_server);

    tpool_destroy(p_server->p_tm);
    p_server->p_tm = NULL;

    registry_destroy(p_server->p_registry);
    p_server->p_registry = NULL;

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
    g_keep_running = 0;
    return;
}

static void
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

    if (NULL == p_server)
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
    struct epoll_event client_ev;
    memset(&client_ev, 0, sizeof(client_ev));
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

static client_t *
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

    p_client = malloc(sizeof(*p_client));
    if (NULL == p_client)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Initialise to safe sentinel values
    p_client->rport        = 0u;
    p_client->p_rhost      = NULL;
    p_client->sockfd       = -1;
    p_client->p_clientdata = NULL;

    struct sockaddr_in client_addr;
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
            status = g_keep_running ?
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

    p_rhost = malloc(INET_ADDRSTRLEN);
    if (NULL == p_rhost)
    {
        fprintf(stderr, "malloc failed\n");
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

static status_t
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

static registry_t *
registry_create (void)
{
    status_t status = STATUS_SUCCESS;

    registry_t * p_registry = malloc(sizeof(*p_registry));
    if (NULL == p_registry)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_registry->pp_clients = malloc(
        max_clients * sizeof(*(p_registry->pp_clients))
    );
    if (NULL == p_registry->pp_clients)
    {
        fprintf(stderr, "malloc failed\n");
        free(p_registry);
        p_registry = NULL;
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memset(
        p_registry->pp_clients,
        0,
        max_clients * sizeof(*(p_registry->pp_clients))
    );

    if (0 != pthread_mutex_init(&(p_registry->lock), NULL))
    {
        perror("pthread_mutex_init");
        free(p_registry->pp_clients);
        p_registry->pp_clients = NULL;
        free(p_registry);
        p_registry = NULL;
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

static status_t
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
    for (size_t idx = 0u; idx < max_clients; idx++)
    {
        if (NULL == (p_registry->pp_clients)[idx])
        {
            (p_registry->pp_clients)[idx] = p_client;
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

static status_t
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
    for (size_t idx = 0u; idx < max_clients; idx++)
    {
        if (NULL == (p_registry->pp_clients)[idx])
        {
            continue;
        }

        if (p_client->sockfd == ((p_registry->pp_clients)[idx])->sockfd)
        {
            (p_registry->pp_clients)[idx] = NULL;
            status = STATUS_SUCCESS;
            break;
        }
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

static status_t
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
    for (size_t idx = 0u; idx < max_clients; idx++)
    {
        client_t * p_client = (p_registry->pp_clients)[idx];
        if (NULL == p_client)
        {
            continue;
        }

        if (-1 == shutdown(p_client->sockfd, SHUT_RDWR))
        {
            perror("shutdown in registry_shutdown");
        }
    }
    pthread_mutex_unlock(&(p_registry->lock));

cleanup:
    return status;
}

static status_t
registry_cleanup_clients (server_t * p_server)
{
    status_t status = STATUS_SUCCESS;

    if ((NULL == p_server) || (NULL == p_server->p_registry))
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    registry_t * p_registry = p_server->p_registry;

    /*
    NOTE:
    Can't call client_destroy while locked because
    client_destroy calls registry_remove which also locks
    */

    // Collect whatever remains under the lock, then destroy outside it
    client_t ** pp_remaining = calloc(max_clients, sizeof(*pp_remaining));
    if (NULL == pp_remaining)
    {
        fprintf(stderr, "calloc failed in registry_cleanup_clients\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    size_t remaining_count = 0u;

    pthread_mutex_lock(&(p_registry->lock));
    for (size_t idx = 0u; idx < max_clients; idx++)
    {
        if (NULL != (p_registry->pp_clients)[idx])
        {
            pp_remaining[remaining_count] = (p_registry->pp_clients)[idx];
            (p_registry->pp_clients)[idx] = NULL;
            remaining_count++;
        }
    }
    pthread_mutex_unlock(&(p_registry->lock));

    // Destroy each idle client
    for (size_t idx = 0u; idx < remaining_count; idx++)
    {
        client_t * p_client = pp_remaining[idx];

        if (NULL != p_server->p_client_free)
        {
            (p_server->p_client_free)(p_server, p_client);
            p_client->p_clientdata = NULL;
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
                perror("epoll_ctl DEL in registry_cleanup_clients");
            }
        }

        if (-1 == shutdown(p_client->sockfd, SHUT_RDWR))
        {
            if (ENOTCONN != errno)
            {
                perror("shutdown in registry_cleanup_clients");
            }
        }

        if (-1 == close(p_client->sockfd))
        {
            perror("close in registry_cleanup_clients");
        }

        free(p_client->p_rhost);
        p_client->p_rhost = NULL;
        free(p_client);
        pp_remaining[idx] = NULL;
    }

    free(pp_remaining);
    pp_remaining = NULL;

cleanup:
    return status;
}

static status_t
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
