
/*
 * $Id$
 *
 * Author: Jeff Hill
 */

#ifndef osdTimeh
#define osdTimeh

struct timespec {
	time_t tv_sec; /* seconds since some epoch */
	long tv_nsec; /* nanoseconds within the second */
};

struct tm *gmtime_r (const time_t *, struct tm *);
struct tm *localtime_r (const time_t *, struct tm *);

#endif /* ifndef osdTimeh */