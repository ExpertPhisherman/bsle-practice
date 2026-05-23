/** @file sockutil.c
 *
 * @brief Socket utility source
 *
 * @par
 *
 */

#include "sockutil.h"

extern _Atomic bool gb_running;

status_t
sockutil_sendall (int sockfd, void * p_buf, size_t size)
{
    status_t status = STATUS_SUCCESS;

    size_t total = 0u;

    while (total < size)
    {
        ssize_t sent = send(
            sockfd,
            (uint8_t *)p_buf + total,
            size - total,
            MSG_NOSIGNAL
        );
        if (-1 == sent)
        {
            if (EINTR == errno)
            {
                if (!gb_running)
                {
                    status = STATUS_SERVER_DISCONNECT;
                    goto cleanup;
                }
                continue;
            }

            perror("sendall");
            status = STATUS_SEND_FAILURE;
            goto cleanup;
        }
        else if (0 == sent)
        {
            perror("sendall");
            status = STATUS_SEND_FAILURE;
            goto cleanup;
        }
        else
        {
            total += (size_t)sent;
        }
    }

cleanup:
    return status;
}

status_t
sockutil_recvall (int sockfd, void * p_buf, size_t size)
{
    status_t status = STATUS_SUCCESS;

    size_t total = 0u;

    while (total < size)
    {
        ssize_t recvd = recv(sockfd, (uint8_t *)p_buf + total, size - total, 0);
        if (-1 == recvd)
        {
            if (EINTR == errno)
            {
                if (!gb_running)
                {
                    status = STATUS_SERVER_DISCONNECT;
                    goto cleanup;
                }
                continue;
            }

            perror("recvall");
            status = STATUS_RECV_FAILURE;
            goto cleanup;
        }
        else if (0 == recvd)
        {
            status = gb_running ?
                STATUS_CLIENT_DISCONNECT :
                STATUS_SERVER_DISCONNECT;
            goto cleanup;
        }
        else
        {
            total += (size_t)recvd;
        }
    }

cleanup:
    return status;
}

status_t
sockutil_drain (int sockfd, size_t size, size_t chunk_size)
{
    status_t status = STATUS_SUCCESS;

    uint8_t * p_buf = NULL;

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

cleanup:
    free(p_buf);
    p_buf = NULL;

    return status;
}

/*** end of file ***/
