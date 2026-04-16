/** @file main.h
 *
 * @brief Main header
 *
 */

#ifndef MAIN_H
#define MAIN_H

#include "server.h"
#include "common.h"
#include "tpool.h"

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
