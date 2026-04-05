/** @file stack.h
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef STACK_H
#define STACK_H

// NOTE: Change accordingly
typedef uint16_t stack_data_t;

typedef struct node
{
    stack_data_t * p_data;
    struct node * p_next;
} node;

/*!
 * @brief Create stack
 *
 * @param[in] p_node_count Pointer to number of nodes
 *
 * @return Pointer to top node
 */
node * stack_init(size_t * p_node_count);

/*!
 * @brief Display stack
 *
 * @param[in] p_top        Pointer to top node
 * @param[in] p_node_count Pointer to number of nodes
 * @param[in] format       Format specifier
 *
 * @return void
 */
void stack_display(node * p_top, const size_t * p_node_count, const char * format);

/*!
 * @brief Push to stack
 *
 * @param[in] p_top        Pointer to top node
 * @param[in] p_node_count Pointer to number of nodes
 * @param[in] data         Data to push
 *
 * @return Pointer to top node
 */
node * stack_push(node * p_top, size_t * p_node_count, const stack_data_t data);

/*!
 * @brief Pop from stack
 *
 * @param[in] p_top        Pointer to top node
 * @param[in] p_node_count Pointer to number of nodes
 *
 * @return Pointer to top node
 */
node * stack_pop(node * p_top, size_t * p_node_count);

/*!
 * @brief Destroy stack
 *
 * @param[in] p_top Pointer to top node
 *
 * @return Pointer to top node
 */
node * stack_destroy(node * p_top, size_t * p_node_count);

/*!
 * @brief Main
 *
 * @param[in] void
 *
 * @return Exit code
 */
int main(void);

#endif /* STACK_H */

/*** end of file ***/
