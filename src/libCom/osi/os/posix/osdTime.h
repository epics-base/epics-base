
/*
 * $Id$
 *
 * Author: Jeff Hill
 */

#ifndef osdTimeh
#define osdTimeh

#include <unistd.h>

#ifndef _POSIX_TIMERS
	struct timespec {
		time_t tv_sec; /* seconds since some epoch */
		long tv_nsec; /* nanoseconds within the second */
	};
#endif /* ifndef _POSIX_TIMERS */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
epicsShareFunc void epicsShareAPI
    convertDoubleToWakeTime(double timeout,struct timespec *wakeTime);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ifndef osdTimeh */

