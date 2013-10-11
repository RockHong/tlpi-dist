/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2010.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU Lesser General Public License as published   *
* by the Free Software Foundation, either version 3 or (at your option)   *
* any later version. This program is distributed without any warranty.    *
* See the files COPYING.lgpl-v3 and COPYING.gpl-v3 for details.           *
\*************************************************************************/

/* Listing 61-1 */

/* rdwrn.c

   Implementations of readn() and writen().
*/
#include <unistd.h>
#include <errno.h>
#include "rdwrn.h"                      /* Declares readn() and writen() */

/* Read 'n' bytes from 'fd' into 'buf', restarting after partial
   reads or interruptions by a signal handlers */

/* hong: try best to read n, it maybe still partially read.*/
ssize_t
readn(int fd, void *buffer, size_t n)
{
    ssize_t numRead;                    /* # of bytes fetched by last read() */
    size_t totRead;                     /* Total # of bytes read so far */
    char *buf;

    buf = buffer;                       /* No pointer arithmetic on "void *" */
    for (totRead = 0; totRead < n; ) {
        numRead = read(fd, buf, n - totRead);

/* hong: p79, read(), if return 0, mean EOF. */
        if (numRead == 0)               /* EOF */
            return totRead;             /* May be 0 if this is first read() */
        if (numRead == -1) {
/* hong: if interrupted by signal, then continue loop.
 * actually, when set up signal hanlder, we can use SA_RESTART. 21.5
 * if the option is set, the read will restart if it's partially done and interrupted
 * by the signal.
 * i'm not sure if SA_RESTART is an option which turns on be default or not.
 * and, maybe it's not convenient & possible to set this option for every signal.
 * so we can't rely on SA_RESTART.
 */
            if (errno == EINTR)
                continue;               /* Interrupted --> restart read() */
            else
                return -1;              /* Some other error */
        }
        totRead += numRead;
        buf += numRead;
    }
    return totRead;                     /* Must be 'n' bytes if we get here */
}

/* Write 'n' bytes to 'fd' from 'buf', restarting after partial
   write or interruptions by a signal handlers */

ssize_t
writen(int fd, const void *buffer, size_t n)
{
    ssize_t numWritten;                 /* # of bytes written by last write() */
    size_t totWritten;                  /* Total # of bytes written so far */
    const char *buf;

    buf = buffer;                       /* No pointer arithmetic on "void *" */
    for (totWritten = 0; totWritten < n; ) {
        numWritten = write(fd, buf, n - totWritten);

        /* The "write() returns 0" case should never happen, but the
           following ensures that we don't loop forever if it does */

        if (numWritten <= 0) {
            if (numWritten == -1 && errno == EINTR)
                continue;               /* Interrupted --> restart write() */
            else
                return -1;              /* Some other error */
        }
        totWritten += numWritten;
        buf += numWritten;
    }
    return totWritten;                  /* Must be 'n' bytes if we get here */
}
