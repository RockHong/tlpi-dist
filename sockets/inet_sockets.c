/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2010.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU Lesser General Public License as published   *
* by the Free Software Foundation, either version 3 or (at your option)   *
* any later version. This program is distributed without any warranty.    *
* See the files COPYING.lgpl-v3 and COPYING.gpl-v3 for details.           *
\*************************************************************************/

/* Listing 59-9 */

/* inet_sockets.c

   A package of useful routines for Internet domain sockets.
*/
#define _BSD_SOURCE             /* To get NI_MAXHOST and NI_MAXSERV
                                   definitions from <netdb.h> */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "inet_sockets.h"       /* Declares functions defined here */
#include "tlpi_hdr.h"

/* The following arguments are common to several of the routines
   below:

        'host':         NULL for loopback IP address, or
                        a host name or numeric IP address
        'service':      either a name or a port number
        'type':         either SOCK_STREAM or SOCK_DGRAM
*/

/* Create socket and connect it to the address specified by
  'host' + 'service'/'type'. Return socket descriptor on success,
  or -1 on error */

int
inetConnect(const char *host, const char *service, int type)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;

    /* hong: set fields of hints to 0 or NULL */
    memset(&hints, 0, sizeof(struct addrinfo));
    /* hong: make pointer be NULL, not simply 0 in case NULL is not defined as 0 */
    hints.ai_canonname = NULL; 
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_family = AF_UNSPEC;        /* Allows IPv4 or IPv6 */
    hints.ai_socktype = type;

    s = getaddrinfo(host, service, &hints, &result);
    if (s != 0) {
        errno = ENOSYS;
        return -1;
    }

    /* Walk through returned list until we find an address structure
       that can be used to successfully connect a socket */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;                   /* On error, try next address */

/* hong: udp can also use connect(), 56.6.2
 * after call connect(), a udp socket can
 * - use write(), read()
 * - only receive msg sent by peer socket specified in connect()
 */
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                      /* Success */

        /* Connect failed: close this socket and try next address */

        close(sfd);
    }

    freeaddrinfo(result);

    return (rp == NULL) ? -1 : sfd;
}

/* Create an Internet domain socket and bind it to the address
   { wildcard-IP-address + 'service'/'type' }.
   If 'doListen' is TRUE, then make this a listening socket (by
   calling listen() with 'backlog'), with the SO_REUSEADDR option set.
   If 'addrLen' is not NULL, then use it to return the size of the
   address structure for the address family for this socket.
   Return the socket descriptor on success, or -1 on error. */

static int              /* Public interfaces: inetBind() and inetListen() */
inetPassiveSocket(const char *service, int type, socklen_t *addrlen,
                  Boolean doListen, int backlog)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, optval, s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_socktype = type;
    hints.ai_family = AF_UNSPEC;        /* Allows IPv4 or IPv6 */
/* hong: p1216
 * wildcard IP address: p1187, usually 0.0.0.0. use it, we can listen to all ip
 * of a machine( a machine may have many network device)
 */
    hints.ai_flags = AI_PASSIVE;        /* Use wildcard IP address */

/* hong: 1st argu: should be NULL, if AI_PASSIVE is set. */
    s = getaddrinfo(NULL, service, &hints, &result);
    if (s != 0)
        return -1;

    /* Walk through returned list until we find an address structure
       that can be used to successfully create and bind a socket */

    optval = 1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;                   /* On error, try next address */

        if (doListen) {
/* hong: p1278, 
 * 2nd argu:
 * 3rd argu: 61.10, for most server, we should turn on this option for socket
 * 4th argu: optval = 1, so we turn on the SO_REUSEADDR option
 */
            if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optval,
                    sizeof(optval)) == -1) {
                close(sfd);
/* hong: remember to free the memory used by the return value of getaddrinfo */
                freeaddrinfo(result);
                return -1;
            }
        }

/* hong: no matter tcp or udp, we must use bind to make connection between
 * socket fd and ip adrees & port
 */
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                      /* Success */

        /* bind() failed: close this socket and try next address */

        close(sfd);
    }

    if (rp != NULL && doListen) {
        if (listen(sfd, backlog) == -1) {
            freeaddrinfo(result);
            return -1;
        }
    }

    if (rp != NULL && addrlen != NULL)
        *addrlen = rp->ai_addrlen;      /* Return address structure size */

    freeaddrinfo(result);

    return (rp == NULL) ? -1 : sfd;
}

/* Create stream socket, bound to wildcard IP address + port given in
  'service'. Make the socket a listening socket, with the specified
  'backlog'. Return socket descriptor on success, or -1 on error. */

int
inetListen(const char *service, int backlog, socklen_t *addrlen)
{
    return inetPassiveSocket(service, SOCK_STREAM, addrlen, TRUE, backlog);
}

/* Create socket bound to wildcard IP address + port given in
   'service'. Return socket descriptor on success, or -1 on error. */

int
inetBind(const char *service, int type, socklen_t *addrlen)
{
    return inetPassiveSocket(service, type, addrlen, FALSE, 0);
}

/* Given a socket address in 'addr', whose length is specified in
   'addrlen', return a null-terminated string containing the host and
   service names in the form "(hostname, port#)". The string is
   returned in the buffer pointed to by 'addrStr', and this value is
   also returned as the function result. The caller must specify the
   size of the 'addrStr' buffer in 'addrStrLen'. */

char *
inetAddressStr(const struct sockaddr *addr, socklen_t addrlen,
               char *addrStr, int addrStrLen)
{
    char host[NI_MAXHOST], service[NI_MAXSERV];

    if (getnameinfo(addr, addrlen, host, NI_MAXHOST,
                    service, NI_MAXSERV, NI_NUMERICSERV) == 0)
        snprintf(addrStr, addrStrLen, "(%s, %s)", host, service);
    else
        snprintf(addrStr, addrStrLen, "(?UNKNOWN?)");

    return addrStr;
}
