/** @file sll.c
 *
 * @brief Singly linked list
 *
 */

#include "../include/sll.h"

status_t
sll_create (sll_t * p_sll)
{
    status_t status;

    if (NULL == p_sll)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_sll->p_head = NULL;
    p_sll->size = 0u;

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    return status;
}

status_t
sll_display (sll_t * p_sll)
{
    status_t status;

    if (NULL == p_sll)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (NULL == p_sll->p_head)
    {
        status = STATUS_EMPTY;
        goto cleanup;
    }

    // Traverse nodes
    node_t * p_curr = p_sll->p_head;
    while (NULL != p_curr)
    {
        printf("%s ", *(sll_data_t*)(p_curr->p_data));

        // Update current node
        p_curr = p_curr->p_next;
    }
    printf("\n");

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    return status;
}

bool
sll_in (sll_t * p_sll, sll_data_t const data)
{
    bool b_data_in;

    if (NULL == p_sll)
    {
        b_data_in = false;
        goto cleanup;
    }

    // Traverse nodes until data is found
    node_t * p_curr = p_sll->p_head;
    while (NULL != p_curr)
    {
        // Compare passed in data to node data
        if (0 == memcmp(p_curr->p_data, &data, sizeof(data)))
        {
            b_data_in = true;
            goto cleanup;
        }

        // Update current node
        p_curr = p_curr->p_next;
    }

    b_data_in = false;
    goto cleanup;

cleanup:
    return b_data_in;
}

status_t
sll_insert (sll_t * p_sll, sll_data_t const data, size_t index)
{
    status_t status;

    if (NULL == p_sll)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    if (index > p_sll->size)
    {
        status = STATUS_OVERFLOW;
        goto cleanup;
    }

    node_t * p_prev = NULL;
    node_t * p_curr = p_sll->p_head;
    while (0u < index)
    {
        // Update previous node
        p_prev = p_curr;

        // Update current node
        p_curr = p_curr->p_next;

        // Decrement index
        index--;
    }

    // Allocate node
    node_t * p_target = malloc(sizeof(node_t));
    if (NULL == p_target)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Allocate node data
    p_target->p_data = malloc(sizeof(sll_data_t));
    if (NULL == p_target->p_data)
    {
        if (NULL != p_target)
        {
            free(p_target);
        }

        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    // Write node data
    memcpy(p_target->p_data, &data, sizeof(data));

    // Previous node links to inserted node
    if (NULL == p_prev)
    {
        p_sll->p_head = p_target;
    }
    else
    {
        p_prev->p_next = p_target;
    }

    // Inserted node links to current node
    p_target->p_next = p_curr;

    // Increment size
    (p_sll->size)++;

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    if (STATUS_ALLOC_FAILURE == status)
    {
        sll_destroy(p_sll);
    }

    return status;
}

status_t
sll_remove (sll_t * p_sll, sll_data_t const data)
{
    status_t status;

    if (NULL == p_sll)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Traverse nodes until data is found
    node_t * p_prev = NULL;
    node_t * p_curr = p_sll->p_head;

    while (NULL != p_curr)
    {
        // Compare node data to passed in data
        if (0 == memcmp(p_curr->p_data, &data, sizeof(data)))
        {
            // Link skips node where data was found
            if (NULL == p_prev)
            {
                p_sll->p_head = p_curr->p_next;
            }
            else
            {
                p_prev->p_next = p_curr->p_next;
            }

            // Free node where data was found
            if (NULL != (p_curr->p_data))
            {
                free(p_curr->p_data);
                p_curr->p_data = NULL;
            }
            if (NULL != p_curr)
            {
                free(p_curr);
                p_curr = NULL;
            }

            // Decrement size
            (p_sll->size)--;

            // NOTE: Data found in SLL
            status = STATUS_SUCCESS;
            goto cleanup;
        }

        // Update previous node
        p_prev = p_curr;

        // Update current node
        p_curr = p_curr->p_next;
    }

    // NOTE: Data not found in SLL
    status = STATUS_NOT_EXISTS;
    goto cleanup;

cleanup:
    return status;
}

status_t
sll_destroy (sll_t * p_sll)
{
    status_t status;

    if (NULL == p_sll)
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    // Traverse nodes
    node_t * p_curr = p_sll->p_head;
    while (NULL != p_curr)
    {
        // Free current node
        node_t * p_next = p_curr->p_next;
        if (NULL != (p_curr->p_data))
        {
            free(p_curr->p_data);
            p_curr->p_data = NULL;
        }
        if (NULL != p_curr)
        {
            free(p_curr);
            p_curr = NULL;
        }

        // Update current node
        p_curr = p_next;
    }

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    p_sll->p_head = NULL;
    p_sll->size = 0u;

    return status;
}

#ifdef SLL_TEST
int
main (void)
{
    status_t status;
    sll_t sll;
    size_t index;

    // Create SLL
    status = sll_create(&sll);
    if (STATUS_NULL_ARG == status)
    {
        printf("NULL argument is present.\n");
        goto cleanup;
    }

    // Insert data
    sll_data_t data_arr[] = {"lime", "lemon", "apple", "orange7", "orange", "banana"};
    size_t size = sizeof(data_arr) / sizeof(sll_data_t);
    for (index = 0u; index < size; index++)
    {
        status = sll_insert(&sll, data_arr[index], index);
        if (STATUS_NULL_ARG == status)
        {
            printf("NULL argument is present.\n");
            goto cleanup;
        }
    }

    // Insert node into SLL if index in range
    status = sll_insert(&sll, "grapefruit", 6u);
    if (STATUS_NULL_ARG == status)
    {
        printf("NULL argument is present.\n");
        goto cleanup;
    }
    else if (STATUS_OVERFLOW == status)
    {
        printf("Index out of range!\n");
        goto cleanup;
    }
    else if (STATUS_ALLOC_FAILURE == status)
    {
        printf("Failed while allocating memory!\n");
        goto cleanup;
    }

    // Remove first instance of data from SLL if data exists
    status = sll_remove(&sll, "lemon");
    if (STATUS_NULL_ARG == status)
    {
        printf("NULL argument is present.\n");
        goto cleanup;
    }
    else if (STATUS_NOT_EXISTS == status)
    {
        printf("Data not in SLL!\n");
        goto cleanup;
    }

    // Display SLL
    status = sll_display(&sll);
    if (STATUS_NULL_ARG == status)
    {
        printf("NULL argument is present.\n");
        goto cleanup;
    }
    else if (STATUS_EMPTY == status)
    {
        printf("SLL is empty.\n");
    }

    // Find first instance of data in SLL
    sll_data_t data = "lime";
    bool b_data_in = sll_in(&sll, data);
    printf("\"%s\" %sin SLL.\n", data, b_data_in ? "" : "not ");

    status = STATUS_SUCCESS;
    goto cleanup;

cleanup:
    // Destroy SLL
    sll_destroy(&sll);

    return exit_code;
}
#endif

/*** end of file ***/
