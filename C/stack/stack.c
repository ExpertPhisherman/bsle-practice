/** @file stack.c
 *
 * @brief (U) Demonstrate skill in creating and using a stack that accepts any data type:
 *
 * @par
 * Create a stack (cannot be fixed sized)
 * Adding an item in a stack (enforce FILO)
 * Removing n items from a stack
 * Removing all items from the stack
 * Destroying a stack
 * Preventing a stack overrun
 */

#include "stack.h"

node *
stack_init(size_t * p_node_count)
{
    node * status = NULL;
    node * p_top = malloc(sizeof(node));
    if (NULL == p_top)
    {
        goto cleanup;
    }

    p_top->p_data = malloc(sizeof(stack_data_t));
    if (NULL == p_top->p_data)
    {
        free(p_top);
        goto cleanup;
    }

    p_top->p_next = NULL;
    *p_node_count = 0u;

    status = p_top;
    goto cleanup;

cleanup:
    return status;
}

void
stack_display (node * p_top, const size_t * p_node_count, const char * format)
{
    if ((NULL == p_top) || (0u == *p_node_count))
    {
        printf("Stack is empty\n");
        goto cleanup;
    }

    node * p_curr = p_top;
    do
    {
        printf(format, *(p_curr->p_data));
        p_curr = p_curr->p_next;
    } while (NULL != p_curr);

cleanup:
    return;
}

node *
stack_push (node * p_top, size_t * p_node_count, const stack_data_t data)
{
    node * status = NULL;
    if (NULL == p_top)
    {
        goto cleanup;
    }

    // Stack is initialized but doesn't hold any data
    if (0u == *p_node_count)
    {
        // First instance of data
        *(p_top->p_data) = data;
    }
    else
    {
        // Preventing a stack overrun
        size_t size_max = SIZE_MAX;
        if (size_max == *p_node_count)
        {
            printf("Stack overflow! Size cannot exceed max: %llu\n", size_max);
            status = p_top;
            goto cleanup;
        }

        node * p_target = malloc(sizeof(node));
        if (NULL == p_target)
        {
            goto cleanup;
        }

        p_target->p_data = malloc(sizeof(stack_data_t));
        if (NULL == p_target->p_data)
        {
            free(p_target);
            goto cleanup;
        }

        *(p_target->p_data) = data;
        p_target->p_next = p_top;
        p_top = p_target;
    }
    (*p_node_count)++;

    status = p_top;
    goto cleanup;

cleanup:
    return status;
}

node *
stack_pop (node * p_top, size_t * p_node_count)
{
    node * status = NULL;
    if (NULL == p_top)
    {
        goto cleanup;
    }

    node * p_target = p_top;
    // Set new top
    p_top = p_target->p_next;
    free(p_target->p_data);
    free(p_target);
    if (0u < *p_node_count)
    {
        (*p_node_count)--;
    }

    status = p_top;
    goto cleanup;

cleanup:
    return status;
}

node *
stack_destroy (node * p_top, size_t * p_node_count)
{
    while (NULL != p_top)
    {
        p_top = stack_pop(p_top, p_node_count);
    }
    p_top = NULL;

    return p_top;
}

int
main (void)
{
    size_t node_count;

    // Create a stack (cannot be fixed sized)
    node * p_top = stack_init(&node_count);

    // Adding an item in a stack (enforce FILO)
    p_top = stack_push(p_top, &node_count, 23u);
    p_top = stack_push(p_top, &node_count, 100u);
    p_top = stack_push(p_top, &node_count, 7891u);
    p_top = stack_push(p_top, &node_count, 22222u);

    // Removing n items from a stack
    size_t pop_count = 2u;
    for (size_t pop_index = 0u; pop_index < pop_count; pop_index++)
    {
        p_top = stack_pop(p_top, &node_count);
    }

    stack_display(p_top, &node_count, "%u\n");

    // Removing all items from the stack
    // Destroying a stack
    p_top = stack_destroy(p_top, &node_count);

    return 0;
}

/*** end of file ***/
