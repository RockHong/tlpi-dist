/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2010.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU Affero General Public License as published   *
* by the Free Software Foundation, either version 3 or (at your option)   *
* any later version. This program is distributed without any warranty.    *
* See the file COPYING.agpl-v3 for details.                               *
\*************************************************************************/

/* Listing 59-6 */

/* is_seqnum_sv.c

   A simple Internet stream socket server. Our service is to provide
   unique sequence numbers to clients.

   Usage:  is_seqnum_sv [init-seq-num]
                        (default = 0)

   See also is_seqnum_cl.c.
*/
#define _BSD_SOURCE             /* To get definitions of NI_MAXHOST and
                                   NI_MAXSERV from <netdb.h> */
#include <netdb.h>
#include "is_seqnum.h"

#define BACKLOG 50

int
main(int argc, char *argv[])
{
/* hong: uint32_t is from C99, in "stdint.h"
 * use this kind of type, we know the width of an int
 * http://stackoverflow.com/questions/6013245/are-types-like-uint32-int32-uint64-int64-defined-in-any-stdlib-header
 * http://cboard.cprogramming.com/c-programming/88031-uint32_t-uint32.html
 */
    uint32_t seqNum;
    char reqLenStr[INT_LEN];            /* Length of requested sequence */
    char seqNumStr[INT_LEN];            /* Start of granted sequence */
/* hong: p1204, big enough to hold any type of socket address,
 * including ipv4 or ipv6 socket address structure,
 * you can also use it for unix domain socket, i think.
 * */
    struct sockaddr_storage claddr;
/* hong: socket fd use type 'int' */
    int lfd, cfd, optval, reqLen;
/* hong: socklen_t, p1154, */
    socklen_t addrlen;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
#define ADDRSTRLEN (NI_MAXHOST + NI_MAXSERV + 10)
    char addrStr[ADDRSTRLEN];
/* hong: p1218, NI_MAXHOST, NI_MAXSERV, is defined in netdb.h. 
 * they tell the max length for host and service name
 */
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];

    if (argc > 1 && strcmp(argv[1], "--help") == 0)
        usageErr("%s [init-seq-num]\n", argv[0]);

    seqNum = (argc > 1) ? getInt(argv[1], 0, "init-seq-num") : 0;

    /* Ignore the SIGPIPE signal, so that we find out about broken connection
       errors via a failure from write(). */

/* hong: p1220,
 * 'This prevents the server from receiving the SIGPIPE
 * signal if it tries to write to a socket whose peer has been closed; instead, the
 * write() fails with the error EPIPE.'
 * 
 * SIGPIPE, p392, p396
 * if you don't ignore it, it will cause termination
 */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        errExit("signal");

    /* Call getaddrinfo() to obtain a list of addresses that
       we can try binding to */

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;        /* Allows IPv4 or IPv6 */
/* hong: p1216, we set AI_PASSIVE flag and host name NULL for getaddrinfo(),
 * so we expect to get a wildcard ip address for listening
 */
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
                        /* Wildcard IP address; service name is numeric */

    if (getaddrinfo(NULL, PORT_NUM, &hints, &result) != 0)
        errExit("getaddrinfo");

    /* Walk through returned list until we find an address structure
       that can be used to successfully create and bind a socket */

    optval = 1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        lfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (lfd == -1)
            continue;                   /* On error, try next address */

/* hong: usually, we turn on 'reuseaddr option' for a server */
        if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))
                == -1)
             errExit("setsockopt");

        if (bind(lfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                      /* Success */

        /* bind() failed: close this socket and try next address */

        close(lfd);
    }

    if (rp == NULL)
        fatal("Could not bind socket to any address");

/* hong: for tcp, we call listen() after bind() */
    if (listen(lfd, BACKLOG) == -1)
        errExit("listen");

    freeaddrinfo(result);

    for (;;) {                  /* Handle clients iteratively */

        /* Accept a client connection, obtaining client's address */

        addrlen = sizeof(struct sockaddr_storage);
/* hong: 'struct sockaddr', p1154, 56.4
 * ipv4 has address structure sockaddr_in
 * and, ipv6 has sockaddr_in6, p1151
 * 'struct sockaddr' is to store socket address for both ipv4 and ipv6
 * use it, we have one generic bind()/accept() api for both ipv4 and ipv6
 */
        cfd = accept(lfd, (struct sockaddr *) &claddr, &addrlen);
        if (cfd == -1) {
            errMsg("accept");
            continue;
        }

        if (getnameinfo((struct sockaddr *) &claddr, addrlen,
                    host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0)
/* hong: use snprintf() to print a string. */
            snprintf(addrStr, ADDRSTRLEN, "(%s, %s)", host, service);
        else
            snprintf(addrStr, ADDRSTRLEN, "(?UNKNOWN?)");
        printf("Connection from %s\n", addrStr);

        /* Read client request, send sequence number back */

/* hong: todo: readLine()  */
        if (readLine(cfd, reqLenStr, INT_LEN) <= 0) {
            close(cfd);
            continue;                   /* Failed read; skip request */
        }

/* hong: atoi() */
        reqLen = atoi(reqLenStr);
        if (reqLen <= 0) {              /* Watch for misbehaving clients */
            close(cfd);
            continue;                   /* Bad request; skip it */
        }

        snprintf(seqNumStr, INT_LEN, "%d\n", seqNum);
        if (write(cfd, &seqNumStr, strlen(seqNumStr)) != strlen(seqNumStr))
            fprintf(stderr, "Error on write");

        seqNum += reqLen;               /* Update sequence number */

/* hong: if connection finishs, close() it.*/
        if (close(cfd) == -1)           /* Close connection */
            errMsg("close");
    }
}
