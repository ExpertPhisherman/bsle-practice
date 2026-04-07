/** @file main.h
 *
 * @brief Main header
 *
 */

#ifndef MAIN_H
#define MAIN_H

#include "server.h"
#include "sll.h"
#include "tpool.h"
#include "common.h"

volatile sig_atomic_t g_keep_running = 1;

/*!
 * @brief Gracefully shutdown server on SIGINT
 *
 * @param[in] signo Signal number
 *
 * @return void
 */
void handle_sigint(int signo);

#endif /* MAIN_H */

/*** end of file ***/
