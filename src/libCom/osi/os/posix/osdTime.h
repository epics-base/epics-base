
/*
 * $Id$
 *
 * Author: Jeff Hill
 */

#ifndef osdTimeh
#define osdTimeh

#include <unistd.h>

/*
 * "struct timespec" is not in all versions of POSIX 1.
 * Solaris has it at ISO POSIX-1c so we will start from there,
 * but may need to fine tune this
 */
#if _POSIX_VERSION < 199506L
	struct timespec {
		time_t tv_sec; /* seconds since some epoch */
		long tv_nsec; /* nanoseconds within the second */
	};
#endif /* if _POSIX_VERSION < 199506L */

#endif /* ifndef osdTimeh */

