/** @file registry.h
 *
 * @brief Generic TCP client registry header
 *
 * @par
 *
 */

#ifndef REGISTRY_H
#define REGISTRY_H

#include <pthread.h>

#include "common.h"

typedef struct client client_t;

typedef struct registry
{
    client_t        ** pp_clients; // Double pointer to clients
    pthread_mutex_t    lock;       // Mutex lock for read/write control
} registry_t;

/*!
 * @brief Create registry
 *
 * @param[in] void
 *
 * @return Pointer to registry
 */
registry_t * registry_create(void);

/*!
 * @brief Add client to registry
 *
 * @param[in] p_registry Pointer to registry
 * @param[in] p_client   Pointer to client to add
 *
 * @return Status of operation
 */
status_t registry_add(registry_t * p_registry, client_t * p_client);

/*!
 * @brief Remove client from registry
 *
 * @param[in] p_registry Pointer to registry
 * @param[in] p_client   Pointer to client to remove
 *
 * @return Status of operation
 */
status_t registry_remove(registry_t * p_registry, client_t * p_client);

/*!
 * @brief Shutdown all client sockets to unblock workers blocked in recv()
 *
 * @param[in] p_registry Pointer to registry
 *
 * @return Status of operation
 */
status_t registry_shutdown(registry_t * p_registry);

/*!
 * @brief Destroy registry
 *
 * @param[in] p_registry Pointer to registry
 *
 * @return Status of operation
 */
status_t registry_destroy(registry_t * p_registry);

#endif /* REGISTRY_H */

/*** end of file ***/
