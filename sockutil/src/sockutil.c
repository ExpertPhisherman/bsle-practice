/** @file sockutil.c
 *
 * @brief Socket utility source
 *
 * @par
 *
 */

#include "sockutil.h"

extern _Atomic sig_atomic_t g_keep_running;

status_t
sockutil_sendall (int sockfd, void * p_buf, size_t size)
{
    status_t status;

    size_t total = 0u;

    while (total < size)
    {
        ssize_t sent = send(sockfd, (uint8_t *)p_buf + total, size - total, 0);
        if (-1 == sent)
        {
            if (EINTR == errno)
            {
                if (!g_keep_running)
                {
                    status = STATUS_SERVER_DISCONNECT;
                    goto cleanup;
                }
                continue;
            }

            status = STATUS_SEND_FAILURE;
            goto cleanup;
        }
        else if (0 == sent)
        {
            status = STATUS_SEND_FAILURE;
            goto cleanup;
        }
        else
        {
            total += (size_t)sent;
        }
    }

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    if (STATUS_SEND_FAILURE == status)
    {
        perror("sendall");
    }

    return status;
}

status_t
sockutil_recvall (int sockfd, void * p_buf, size_t size)
{
    status_t status;

    size_t total = 0u;

    while (total < size)
    {
        ssize_t recvd = recv(sockfd, (uint8_t *)p_buf + total, size - total, 0);
        if (-1 == recvd)
        {
            if (EINTR == errno)
            {
                if (!g_keep_running)
                {
                    status = STATUS_SERVER_DISCONNECT;
                    goto cleanup;
                }
                continue;
            }

            status = STATUS_RECV_FAILURE;
            goto cleanup;
        }
        else if (0 == recvd)
        {
            status = g_keep_running ? STATUS_CLIENT_DISCONNECT : STATUS_SERVER_DISCONNECT;
            goto cleanup;
        }
        else
        {
            total += (size_t)recvd;
        }
    }

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    if (STATUS_RECV_FAILURE == status)
    {
        perror("recvall");
    }
    else if (STATUS_CLIENT_DISCONNECT == status)
    {
        fprintf(stderr, "Abrupt disconnect from client (sockfd %d)\n", sockfd);
    }
    else
    {
        // Pass
    }

    return status;
}

status_t
sockutil_drain (int sockfd, size_t size, size_t chunk_size)
{
    status_t status = STATUS_SUCCESS;
    uint8_t * p_buf;

    p_buf = malloc(chunk_size);
    if (NULL == p_buf)
    {
        fprintf(stderr, "malloc failed\n");
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    while (0u < size)
    {
        uint32_t chunk = (size < chunk_size) ? size : chunk_size;

        status = sockutil_recvall(sockfd, p_buf, chunk);
        if (STATUS_SUCCESS != status)
        {
            goto cleanup;
        }

        size -= chunk;
    }

    goto cleanup;

cleanup:
    free(p_buf);
    p_buf = NULL;

    return status;
}

/*** end of file ***/
