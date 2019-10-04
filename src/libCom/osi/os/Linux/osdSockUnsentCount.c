/*************************************************************************\
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <linux/sockios.h>
#define EPICS_PRIVATE_API
#include "osiSock.h"

/*
 * epicsSocketUnsentCount ()
 * See https://linux.die.net/man/7/tcp
 */
int epicsSocketUnsentCount(SOCKET sock) {
    int unsent;
    if (ioctl(sock, SIOCOUTQ, &unsent) == 0)
        return unsent;
    return -1;
}
