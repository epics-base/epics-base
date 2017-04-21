/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Author: W. Eric Norum
 */
#define __BSD_VISIBLE 1
#include <sys/types.h>
#include <sys/socket.h>

#include <epicsStdio.h>
#include <rtems.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <rtems/rtems_bsdnet_internal.h>
#include "epicsTime.h"
#include "osdTime.h"
#include "osiNTPTime.h"
#include "osiClockTime.h"
#include "generalTimeSup.h"

extern "C" {

extern rtems_interval rtemsTicksPerSecond;
int rtems_bsdnet_get_ntp(int, int(*)(), struct timespec *);
static int ntpSocket = -1;

void osdTimeRegister(void)
{
    /* Init NTP first so it can be used to sync ClockTime */
    NTPTime_Init(100);
    ClockTime_Init(CLOCKTIME_SYNC);
}

int osdNTPGet(struct timespec *ts)
{
    static unsigned bequiet;
    ssize_t ret;

    if (ntpSocket < 0)
        return -1;

    /* rtems_bsdnet_get_ntp() will send an NTP request, then
     * call recvfrom() exactly once to process the expected reply.
     * Any leftovers in the socket buffer (ie. duplicates of
     * previous replies) will cause problems.
     * So flush out the socket buffer first.
     */
    do {
        char junk[16];

        ret = recvfrom(ntpSocket, junk, sizeof(junk), MSG_DONTWAIT, NULL, NULL);
        if (ret == -1 && errno == EAGAIN) {
            break;
        }
        else if (ret == -1) {
            if (!bequiet) {
                printf("osdNTPGet cleaner error: %s\n", strerror(errno));
                bequiet = 1;
            }
            break;
        }
        else {
            bequiet = 0;
        }
    } while (ret > 0);

    return rtems_bsdnet_get_ntp(ntpSocket, NULL, ts);
}

void osdNTPInit(void)
{
    struct sockaddr_in myAddr;

    ntpSocket = socket (AF_INET, SOCK_DGRAM, 0);
    if (ntpSocket < 0) {
        printf("osdNTPInit() Can't create socket: %s\n", strerror (errno));
        return;
    }
    memset (&myAddr, 0, sizeof myAddr);
    myAddr.sin_family = AF_INET;
    myAddr.sin_port = htons (0);
    myAddr.sin_addr.s_addr = htonl (INADDR_ANY);
    if (bind (ntpSocket, (struct sockaddr *)&myAddr, sizeof myAddr) < 0) {
        printf("osdNTPInit() Can't bind socket: %s\n", strerror (errno));
        close (ntpSocket);
        ntpSocket = -1;
    }
}

void osdNTPReport(void)
{
}

int osdTickGet(void)
{
    rtems_interval t;
    rtems_clock_get (RTEMS_CLOCK_GET_TICKS_SINCE_BOOT, &t);
    return t;
}

int osdTickRateGet(void)
{
    return rtemsTicksPerSecond;
}

/*
 * Use reentrant versions of time access
 */
int epicsTime_gmtime ( const time_t *pAnsiTime, struct tm *pTM )
{
    struct tm * pRet = gmtime_r ( pAnsiTime, pTM );
    if ( pRet ) {
        return epicsTimeOK;
    }
    else {
        return errno;
    }
}

int epicsTime_localtime ( const time_t *clock, struct tm *result )
{
    struct tm * pRet = localtime_r ( clock, result );
    if ( pRet ) {
        return epicsTimeOK;
    }
    else {
        return errno;
    }
}

rtems_interval rtemsTicksPerSecond;
double rtemsTicksPerSecond_double, rtemsTicksPerTwoSeconds_double;

} // extern "C"

/*
 * Static constructors are run too early in a standalone binary
 * to be able to initialize the NTP time provider (the network
 * is not available yet), so the RTEMS standalone startup code
 * explicitly calls osdTimeRegister() at the appropriate time.
 * However if we are loaded dynamically we *do* register our
 * standard time providers at static constructor time; in this
 * case the network is available already.
 */
static int staticTimeRegister(void)
{
    rtems_clock_get (RTEMS_CLOCK_GET_TICKS_PER_SECOND, &rtemsTicksPerSecond);
    rtemsTicksPerSecond_double = rtemsTicksPerSecond;
    rtemsTicksPerTwoSeconds_double = rtemsTicksPerSecond_double * 2.0;

    /* If networking is already up at the time static constructors
     * are executed then we are probably run-time loaded and it's
     * OK to osdTimeRegister() at this point.
     */
    if (rtems_bsdnet_ticks_per_second != 0)
        osdTimeRegister();

    return 1;
}
static int done = staticTimeRegister();
