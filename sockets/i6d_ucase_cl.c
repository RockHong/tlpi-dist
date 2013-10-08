/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2010.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU Affero General Public License as published   *
* by the Free Software Foundation, either version 3 or (at your option)   *
* any later version. This program is distributed without any warranty.    *
* See the file COPYING.agpl-v3 for details.                               *
\*************************************************************************/

/* Listing 59-4 */

/* i6d_ucase_cl.c

   Client for i6d_ucase_sv.c: send each command-line argument as a datagram to
   the server, and then display the contents of the server's response datagram.
*/
#include "i6d_ucase.h"

int
main(int argc, char *argv[])
{
    struct sockaddr_in6 svaddr;
    int sfd, j;
    size_t msgLen;
    ssize_t numBytes;
    char resp[BUF_SIZE];

    if (argc < 3 || strcmp(argv[1], "--help") == 0)
        usageErr("%s host-address msg...\n", argv[0]);

    /* Create a datagram socket; send to an address in the IPv6 domain */

    sfd = socket(AF_INET6, SOCK_DGRAM, 0);      /* Create client socket */
    if (sfd == -1)
        errExit("socket");

    memset(&svaddr, 0, sizeof(struct sockaddr_in6));
    svaddr.sin6_family = AF_INET6;
    /* hong: convert port num into network byte order.
     * h: host; n: network; s: 'short', uint16_t;
     * why needed?
     * because you pass a remote machine the port num and tell it
     * i want to call that service.
     * so you must store the port num in network order and pass it
     */
    svaddr.sin6_port = htons(PORT_NUM);
    /* hong: argv[1] must be ipv6 presentation because the 1st argu of 
     * inet_pton() is set to AF_INET6..
     * for example, the ::1 is ipv6 version of loopback address.
     * inet_pton() doesn't support host name like 'localhost'.
     * if you want to get address from host name, use getaddrinfo() instead.
     * inet_pton() supports:
     * 204.152.189.116 (IPv4 dotted-decimal address);
     * ::1 (an IPv6 colon-separated hexadecimal address); or
     * ::FFFF:204.152.189.116 (an IPv4-mapped IPv6 address)
     */
    if (inet_pton(AF_INET6, argv[1], &svaddr.sin6_addr) <= 0)
        fatal("inet_pton failed for address '%s'", argv[1]);

    /* Send messages to server; echo responses on stdout */

    /* hong: it's not necessary to call connect() before sendto().
     * but it can call connect() if you want. 56.6.2
     * after call connect() to connect to the peer socket in remote,
     * - you can use write(), send()
     * - on the socket fd, you can ONLY receive message from the connected
     *   peer socket
     */
    for (j = 2; j < argc; j++) {
        msgLen = strlen(argv[j]);
        /* hong: 1st argu: the socket fd who sends the message
         * 4th argu: possible value other than 0 see 61.3
         * 5th argu: type of this argu is 'struct sockaddr *'
         * the type of svaddr is sockaddr_in6, so cast it
         */
        if (sendto(sfd, argv[j], msgLen, 0, (struct sockaddr *) &svaddr,
                    sizeof(struct sockaddr_in6)) != msgLen)
            fatal("sendto");

        numBytes = recvfrom(sfd, resp, BUF_SIZE, 0, NULL, NULL);
        if (numBytes == -1)
            errExit("recvfrom");

        printf("Response %d: %.*s\n", j - 1, (int) numBytes, resp);
    }

    exit(EXIT_SUCCESS);
}
