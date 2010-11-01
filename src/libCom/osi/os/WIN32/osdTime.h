/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * $Revision-Id$
 *
 * Author: Jeff Hill
 */

#ifndef osdTimeh
#define osdTimeh

#if ! defined(_MINGW) || ! defined(_TIMESPEC_DEFINED)
struct timespec {
	time_t tv_sec; /* seconds since some epoch */
	long tv_nsec; /* nanoseconds within the second */
};
#endif /* ! defined(_MINGW) || ! defined(_TIMESPEC_DEFINED) */

#endif /* ifndef osdTimeh */

