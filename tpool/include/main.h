/** @file main.h
 *
 * @brief Main header
 *
 */

#ifndef MAIN_H
#define MAIN_H

#define _DEFAULT_SOURCE

#include "tpool.h"
#include "common.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*!
 * @brief Wroker function the pool will call
 *
 * @param[in] p_arg Pointer to argument
 *
 * @return void
 */
void worker(void * p_arg);

#endif /* MAIN_H */

/*** end of file ***/
