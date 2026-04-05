/** @file net.c
 *
 * @brief Network operations
 *
 */

#include "net.h"
#include <stdlib.h>

int
main (void)
{
    int exit_code;
    status_t status;

    struct sockaddr_in sa;   // IPv4
    struct sockaddr_in6 sa6; // IPv6

    //inet_pton(AF_INET, "10.12.110.57", &(sa.sin_addr));
    //inet_pton(AF_INET6, "2001:db8:63b3:1::3490", &(sa6.sin6_addr));

    exit_code = 0;
    goto cleanup;

cleanup:
    return exit_code;
}

/*** end of file ***/
