/** @file C51.c
 *
 * @brief (U) Demonstrate skill in creating and using a circularly linked list that accepts any data type:
 *
 * @par
 * Creating a circularly linked list with n number of items
 * Navigating through a circularly linked list
 * Finding the first occurrence of an item in a circularly linked list
 * Sorting the circularly linked list alphanumerically using a function pointer
 * Removing selected items from the circularly linked list
 * Inserting an item into a specific location in a circularly linked list
 * Removing all items from the circularly linked list
 * Destroying a circularly linked list
 * "Circular linked list" will be abbreviated as "CLL" or "cll"
 */

#include "C51.h"

node *
cll_init (const void * p_bytes, const size_t node_count, const size_t data_size)
{
    node * status = NULL;

    if ((NULL == p_bytes) || (0u == node_count) || (0u == data_size))
    {
        goto cleanup;
    }

    // Create CLL
    node * p_prev = NULL;
    node * p_head = NULL;
    for (size_t key = 0u; key < node_count; key++)
    {
        // Allocate node
        node * p_curr = (node*)malloc(sizeof(node));
        if (NULL == p_curr)
        {
            goto cleanup;
        }

        // Allocate node data
        p_curr->p_data = (void*)malloc(data_size);
        if (NULL == p_curr->p_data)
        {
            free(p_curr);
            goto cleanup;
        }

        // Write node data
        memcpy(p_curr->p_data, (const char*)p_bytes + (key * data_size), data_size);

        if (NULL == p_head)
        {
            p_head = p_curr;
        }
        else
        {
            p_prev->p_next = p_curr;
        }

        // Update previous node
        p_prev = p_curr;
    }
    // NOTE: p_prev now points to tail node

    // Tail node links to head node
    p_prev->p_next = p_head;

    status = p_head;
    goto cleanup;

cleanup:
    return status;
}

void
cll_display (node * p_head, const char * p_format)
{
    if (NULL == p_head)
    {
        printf("CLL is empty\n");
        goto cleanup;
    }

    // Traverse nodes
    node * p_curr = p_head;
    do
    {
        printf(p_format, *(cll_data_t*)p_curr->p_data);
        p_curr = p_curr->p_next;
    } while (p_curr != p_head);
    printf("\n");

    goto cleanup;

cleanup:
    return;
}

node *
cll_find (node * p_head, const void * p_data, const size_t data_size, size_t * p_key)
{
    node * status = NULL;

    if ((NULL == p_head) || (NULL == p_data) || (0u == data_size) || (NULL == p_key))
    {
        goto cleanup;
    }

    // Traverse nodes until data is found
    node * p_curr = p_head;
    *p_key = 0;
    do
    {
        // Compare passed in data to node data
        if (0 == memcmp(p_data, p_curr->p_data, data_size))
        {
            status = p_curr;
            break;
        }
        (*p_key)++;
        p_curr = p_curr->p_next;
    } while (p_curr != p_head);

    goto cleanup;

cleanup:
    return status;
}

int
compar (const void * p_data_x, const void * p_data_y)
{
    return strcmp(*(const char * const *)p_data_x, *(const char * const *)p_data_y);
}

node *
cll_sort (node * p_head, const size_t node_count, const size_t data_size, int (* p_compar)(const void * p_data_x, const void * p_data_y))
{
    node * status = NULL;

    if ((NULL == p_head) || (0u == node_count) || (0u == data_size) || (NULL == p_compar))
    {
        goto cleanup;
    }

    // Allocate array to hold data
    void * p_buf = (void*)malloc(node_count * data_size);
    if (NULL == p_buf)
    {
        goto cleanup;
    }

    // Write data to array (inverse of cll_init)
    char * p_data = (char*)p_buf;
    node * p_curr = p_head;
    do
    {
        // Write to array
        memcpy(p_data, p_curr->p_data, data_size);
        p_data += data_size;

        // Update current node
        p_curr = p_curr->p_next;
    } while (p_curr != p_head);

    // Destroy old CLL
    cll_destroy(p_head);

    // Sort array of all data
    qsort(p_buf, node_count, data_size, p_compar);

    // Create new CLL based on sorted array
    node * p_head_new = cll_init(p_buf, node_count, data_size);

    free(p_buf);

    status = p_head_new;
    goto cleanup;

cleanup:
    return status;
}

node *
cll_remove (node * p_head, const void * p_data, const size_t data_size, size_t * p_node_count)
{
    node * status = p_head;

    if ((NULL == p_head) || (NULL == p_data) || (0u == data_size))
    {
        goto cleanup;
    }

    // Traverse nodes until data is found
    node * p_prev = NULL;
    node * p_curr = p_head;
    do
    {
        // Compare passed in data to node data
        if (0 == memcmp(p_data, p_curr->p_data, data_size))
        {
            if (p_curr == p_head)
            {
                // Check if CLL has only 1 node
                if (p_head->p_next == p_head)
                {
                    // Free head node
                    free(p_head->p_data);
                    free(p_head);

                    // NOTE: CLL is empty
                    status = NULL;
                }
                else
                {
                    // Find tail node
                    node * p_tail = p_head;
                    while (p_tail->p_next != p_head)
                    {
                        p_tail = p_tail->p_next;
                    }

                    // Set new head node
                    node * p_head_new = p_head->p_next;

                    // Tail node links to new head node
                    p_tail->p_next = p_head_new;

                    // Free old head
                    free(p_head->p_data);
                    free(p_head);

                    status = p_head_new;
                }
            }
            else
            {
                // Link skips node where data was found
                p_prev->p_next = p_curr->p_next;

                // Free node where data was found
                free(p_curr->p_data);
                free(p_curr);
            }

            // Decrement number of nodes
            (*p_node_count)--;

            break;
        }

        // Update previous node
        p_prev = p_curr;

        // Update current node
        p_curr = p_curr->p_next;
    } while (p_curr != p_head);

    goto cleanup;

cleanup:
    return status;
}

node *
cll_insert (node * p_head, const void * p_data, const size_t data_size, size_t * p_node_count, const size_t * p_key)
{
    node * status = p_head;

    // NOTE: (NULL == p_head) is excluded in order to handle inserting 1 node into an empty CLL
    if ((NULL == p_data) || (0u == data_size) || (NULL == p_key) || (*p_key > *p_node_count))
    {
        goto cleanup;
    }

    node * p_prev = NULL;
    node * p_curr = p_head;
    for (size_t key = 0; key < (*p_node_count + 1); key++)
    {
        if (key == *p_key)
        {
            // Allocate node
            node * p_target = (node*)malloc(sizeof(node));
            if (NULL == p_target)
            {
                goto cleanup;
            }

            // Allocate node data
            p_target->p_data = (void*)malloc(data_size);
            if (NULL == p_target->p_data)
            {
                free(p_target);
                goto cleanup;
            }

            // Write node data
            memcpy(p_target->p_data, (const char*)p_data, data_size);

            if (0u == *p_key)
            {
                // Find tail node
                node * p_tail;
                if (0u == *p_node_count)
                {
                    p_tail = p_target;
                    p_curr = p_target;
                }
                else
                {
                    p_tail = p_head;
                    while (p_tail->p_next != p_head)
                    {
                        p_tail = p_tail->p_next;
                    }
                }

                // Set pointer to previous node to tail node
                p_prev = p_tail;

                // Update new head node
                status = p_target;
            }

            // Previous node links to inserted node
            p_prev->p_next = p_target;

            // Inserted node links to current node
            p_target->p_next = p_curr;

            // Increment number of nodes
            (*p_node_count)++;

            break;
        }

        // Update previous node
        p_prev = p_curr;

        // Update current node
        p_curr = p_curr->p_next;
    }

    goto cleanup;

cleanup:
    return status;
}

void
cll_destroy (node * p_head)
{
    if (NULL == p_head)
    {
        goto cleanup;
    }

    // Traverse nodes
    node * p_curr = p_head;
    do
    {
        // Free current node
        node * p_next = p_curr->p_next;
        free(p_curr->p_data);
        free(p_curr);

        // Update current node
        p_curr = p_next;
    } while (p_curr != p_head);

    goto cleanup;

cleanup:
    return;
}

int
main (void)
{
    cll_data_t data;
    size_t key;
    size_t node_count;

    // NOTE: Change format specifiers in main accordingly
    cll_data_t p_bytes[] = {"lime", "lemon", "apple", "orange7", "orange", "banana"};

    // Create CLL
    node_count = sizeof(p_bytes) / sizeof(cll_data_t);
    node * p_head = cll_init(p_bytes, node_count, sizeof(cll_data_t));

    // Remove first instance of data from CLL if data exists
    data = "lemon";
    p_head = cll_remove(p_head, &data, sizeof(cll_data_t), &node_count);

    // Insert node into CLL if index in range
    data = "grapefruit";
    key = 5;
    p_head = cll_insert(p_head, &data, sizeof(cll_data_t), &node_count, &key);

    // WARNING: Alphanumerical sort only works when cll_data_t is char *
    // Alphanumerically sort CLL
    p_head = cll_sort(p_head, node_count, sizeof(cll_data_t), compar);

    // Display CLL
    cll_display(p_head, "%s ");

    // Find first instance of data in CLL
    data = "lime";
    node * p_target = cll_find(p_head, &data, sizeof(cll_data_t), &key);
    if (NULL != p_target)
    {
        printf("%s found at index %lu\n", *(cll_data_t*)p_target->p_data, key);
    }
    else
    {
        printf("%s not found\n", data);
    }

    // Destroy CLL
    cll_destroy(p_head);

    return 0;
}

/*** end of file ***/
