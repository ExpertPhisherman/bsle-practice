/** @file server.c
 *
 * @brief Generic TCP server
 *
 * @par
 *
 */

#include "server.h"

extern uint32_t const max_clients;
extern uint32_t const worker_threads;

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
 * @brief Wrapper for client_run
 *
 * @param[in] p_arg Pointer to argument
 *
 * @return void
 */
static void client_run_wrapper(void * p_arg);

/*!
 * @brief Create client
 *
 * @param[in] p_server Pointer to server
 *
 * @return Pointer to client
 */
static client_t * client_create(server_t * p_server);

/*!
 * @brief Destroy client
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
 * @brief Remove file descriptor from registry
 *
 * @param[in] p_registry Pointer to registry
 * @param[in] p_client   Pointer to client to remove
 *
 * @return Status of operation
 */
static status_t registry_remove(registry_t * p_registry, client_t * p_client);

/*!
 * @brief Shutdown all clients in registry
 *
 * @param[in] p_registry Pointer to registry
 *
 * @return Status of operation
 */
static status_t registry_shutdown(registry_t * p_registry);

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
    sa_int.sa_flags = 0;
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

    *p_server = *p_hints;

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

    int sockfd;
    struct sockaddr_in server_addr;
    int yes = 1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        perror("socket");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (-1 == setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
    {
        perror("setsockopt");
        if (-1 == close(sockfd))
        {
            perror("close");
        }

        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(p_server->lport);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Catch privileged port as non-root user
    if ((1024u > (p_server->lport)) && (0 != geteuid()))
    {
        fprintf(stderr, "Cannot bind to privileged port %hu [1-1023] as non-root user\n", p_server->lport);
        if (-1 == close(sockfd))
        {
            perror("close");
        }

        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (-1 == bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
    {
        perror("bind");
        if (-1 == close(sockfd))
        {
            perror("close");
        }

        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (-1 == listen(sockfd, p_server->backlog))
    {
        perror("listen");
        if (-1 == close(sockfd))
        {
            perror("close");
        }

        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    if (p_server->b_verbose)
    {
        char p_ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(server_addr.sin_addr.s_addr), p_ipstr, sizeof(p_ipstr));
        printf("Listening on %s:%hu (sockfd %d)\n", p_ipstr, p_server->lport, sockfd);
    }

    p_server->sockfd = sockfd;
    p_server->p_client_run = p_hints->p_client_run;

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

    if (NULL == p_server)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Accept loop
    while (g_keep_running)
    {
        client_t * p_client = client_create(p_server);
        if (NULL == p_client)
        {
            continue;
        }

        // Pack server and client pair into struct
        server_client_pair_t * p_pair = malloc(sizeof(*p_pair));
        if (NULL == p_pair)
        {
            fprintf(stderr, "malloc failed\n");
            status = STATUS_ALLOC_FAILURE;
            goto cleanup;
        }

        p_pair->p_server = p_server;
        p_pair->p_client = p_client;

        if (!tpool_add_work(p_server->p_tm, client_run_wrapper, p_pair))
        {
            fprintf(stderr, "tpool_add_work failed\n");
            client_destroy(p_server, p_pair->p_client);
            free(p_pair);
            p_pair = NULL;
        }

        // NOTE: Current function no longer has ownership
        p_client = NULL;
    }

    if (p_server->b_verbose)
    {
        printf("\nGraceful shutdown on server (sockfd %d)\n", p_server->sockfd);
    }

cleanup:
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

    // Wait for queued work to finish and join all threads
    tpool_wait(p_server->p_tm);
    tpool_destroy(p_server->p_tm);
    p_server->p_tm = NULL;

    registry_destroy(p_server->p_registry);
    p_server->p_registry = NULL;

    if (-1 != p_server->sockfd)
    {
        if (-1 == close(p_server->sockfd))
        {
            perror("close");
            status = STATUS_SOCKET_FAILURE;
        }
    }

    free(p_server);
    p_server = NULL;

cleanup:
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
    server_client_pair_t * p_pair = NULL;

    if (NULL == p_arg)
    {
        goto cleanup;
    }

    p_pair = p_arg;

    if (NULL == p_pair->p_server)
    {
        goto cleanup;
    }

    if (NULL == p_pair->p_server->p_client_run)
    {
        fprintf(stderr, "App not loaded\n");
        goto cleanup;
    }

    (p_pair->p_server->p_client_run)(p_pair->p_server, p_pair->p_client);

cleanup:
    client_destroy(p_pair->p_server, p_pair->p_client);
    free(p_pair);
    p_pair = NULL;
    return;
}

static client_t *
client_create (server_t * p_server)
{
    status_t status = STATUS_SUCCESS;

    client_t * p_client = NULL;

    if (NULL == p_server)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    int sockfd;
    struct sockaddr_in client_addr;

    socklen_t sin_size = sizeof(client_addr);
    sockfd = accept(p_server->sockfd, (struct sockaddr *)&client_addr, &sin_size);
    if (-1 == sockfd)
    {
        if (EINTR == errno)
        {
            status = g_keep_running ? STATUS_SOCKET_FAILURE : STATUS_SERVER_DISCONNECT;
            goto cleanup;
        }

        perror("accept");
        status = STATUS_SOCKET_FAILURE;
        goto cleanup;
    }

    uint16_t rport = ntohs(client_addr.sin_port);

    if (p_server->b_verbose)
    {
        char p_ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr.s_addr), p_ipstr, sizeof(p_ipstr));
        printf("Accepted connection from %s:%hu (sockfd %d)\n", p_ipstr, rport, sockfd);
    }

    p_client = malloc(sizeof(*p_client));
    if (NULL == p_client)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_client->rport = rport;
    p_client->sockfd = sockfd;

    // Register client file descriptor
    if (STATUS_FULL == registry_add(p_server->p_registry, p_client))
    {
        fprintf(stderr, "max_clients reached, rejecting sockfd %d\n", p_client->sockfd);
        if (-1 == shutdown(p_client->sockfd, SHUT_RDWR))
        {
            perror("shutdown");
        }

        status = STATUS_FULL;
        goto cleanup;
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

    // Unregister file descriptor
    if (STATUS_NOT_EXISTS == registry_remove(p_server->p_registry, p_client))
    {
        fprintf(stderr, "sockfd %d not found in registry\n", p_client->sockfd);
    }

    if (-1 == shutdown(p_client->sockfd, SHUT_RDWR))
    {
        perror("shutdown");
    }

    free(p_client);
    p_client = NULL;

cleanup:
    return status;
}

static registry_t *
registry_create (void)
{
    registry_t * p_registry = malloc(sizeof(*p_registry));
    if (NULL == p_registry)
    {
        fprintf(stderr, "malloc failed\n");
        goto cleanup;
    }

    p_registry->pp_clients = malloc(max_clients * sizeof(*(p_registry->pp_clients)));
    if (NULL == p_registry->pp_clients)
    {
        fprintf(stderr, "malloc failed\n");
        free(p_registry);
        p_registry = NULL;

        goto cleanup;
    }

    memset(p_registry->pp_clients, 0, max_clients * sizeof(*(p_registry->pp_clients)));
    if (0 != pthread_mutex_init(&(p_registry->lock), NULL))
    {
        free(p_registry->pp_clients);
        p_registry->pp_clients = NULL;
        free(p_registry);
        p_registry = NULL;

        perror("pthread_mutex_init");
        goto cleanup;
    }

cleanup:
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
            perror("shutdown");
        }

        (p_registry->pp_clients)[idx] = NULL;
    }
    pthread_mutex_unlock(&(p_registry->lock));

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

    // NOTE: Workers should be done
    free(p_registry->pp_clients);
    p_registry->pp_clients = NULL;

    pthread_mutex_destroy(&(p_registry->lock));

    free(p_registry);
    p_registry = NULL;

cleanup:
    return status;
}

/*** end of file ***/
