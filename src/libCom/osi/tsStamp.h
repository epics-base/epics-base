/*
 * ANSI C manipulation of type TS_STAMP
 *
 * Author: Jeff Hill. 
 *
 * Original TS_STAMP definition by Roger Cole
 *
 * Notes:
 * 1) all functions return -1 on failure and 0 on success
 * 2) the implementation of these routines is provided by 
 *      C++ type osiTime
 */

#ifndef tsStamph
#define tsStamph

#include <time.h>

#include "shareLib.h"
#include "epicsTypes.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct timespec;

#define tsStampOK 0
#define tsStampERROR (-1)

#define tsStampEventCurrentTime 0

/*
 * The number of nanoseconds past 0000 Jan 1, 1990, GMT (or UTC).
 */
typedef struct TS_STAMP {
    epicsUInt32    secPastEpoch;   /* seconds since 0000 Jan 1, 1990 */
    epicsUInt32    nsec;           /* nanoseconds within second */
} TS_STAMP;


/*
 * fetch a time stamp
 */
epicsShareFunc int epicsShareAPI tsStampGetCurrent (TS_STAMP *pDest);
epicsShareFunc int epicsShareAPI tsStampGetEvent (TS_STAMP *pDest, unsigned eventNumber);

/*
 * convert to and from ANSI C's "time_t"
 */
epicsShareFunc int epicsShareAPI tsStampToTime_t (time_t *pDest, const TS_STAMP *pSrc);
epicsShareFunc int epicsShareAPI tsStampFromTime_t (TS_STAMP *pDest, time_t src);

/*
 * convert to and from ANSI C's "struct tm" with nano seconds
 */
epicsShareFunc int epicsShareAPI tsStampToTM (struct tm *pDest, unsigned long *pNSecDest, const TS_STAMP *pSrc);
epicsShareFunc int epicsShareAPI tsStampFromTM (TS_STAMP *pDest, const struct tm *pSrc, unsigned long nSecSrc);

/*
 * convert to and from POSIX RT's "struct timespec"
 */
epicsShareFunc int epicsShareAPI tsStampToTimespec (struct timespec *pDest, const TS_STAMP *pSrc);
epicsShareFunc int epicsShareAPI tsStampFromTimespec (TS_STAMP *pDest, const struct timespec *pSrc);

/*
 * arithmetic operations
 */
epicsShareFunc double epicsShareAPI tsStampDiffInSeconds (const TS_STAMP *pLeft, const TS_STAMP *pRight); /* returns *pLeft - *pRight in seconds */
epicsShareFunc void epicsShareAPI tsStampAddSeconds (TS_STAMP *pDest, double secondsToAdd); /* adds seconds to *pDest */

/*
 * comparison operations
 */
epicsShareFunc int epicsShareAPI tsStampEqual (const TS_STAMP *pLeft, const TS_STAMP *pRight); /* returns boolean result */
epicsShareFunc int epicsShareAPI tsStampNotEqual (const TS_STAMP *pLeft, const TS_STAMP *pRight); /* returns boolean result */
epicsShareFunc int epicsShareAPI tsStampLessThan (const TS_STAMP *pLeft, const TS_STAMP *pRight); /* returns boolean result (true if *pLeft < *pRight) */
epicsShareFunc int epicsShareAPI tsStampLessThanEqual (const TS_STAMP *pLeft, const TS_STAMP *pRight); /* returns boolean result (true if *pLeft <= *pRight) */
epicsShareFunc int epicsShareAPI tsStampGreaterThan (const TS_STAMP *pLeft, const TS_STAMP *pRight); /* returns boolean result (true if *pLeft > *pRight) */
epicsShareFunc int epicsShareAPI tsStampGreaterThanEqual (const TS_STAMP *pLeft, const TS_STAMP *pRight); /* returns boolean result (true if *pLeft >= *pRight) */

/*
 * dump current state to standard out
 */
epicsShareFunc void epicsShareAPI tsStampShow (const TS_STAMP *, unsigned interestLevel);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* tsStamph */
