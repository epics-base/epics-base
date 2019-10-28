/*************************************************************************\
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#define EPICS_PRIVATE_API
#include "osiSock.h"

/*
 * epicsSocketUnsentCount ()
 * See https://www.unix.com/man-page/osx/2/setsockopt
 */
int epicsSocketUnsentCount(SOCKET sock) {
    int unsent;
    socklen_t len = sizeof(unsent);
    if (getsockopt(sock, SOL_SOCKET, SO_NWRITE, &unsent, &len) == 0)
        return unsent;
    return -1;
}
