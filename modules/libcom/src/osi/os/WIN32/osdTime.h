/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author: Jeff Hill
 */

#ifndef INC_osdTime_H
#define INC_osdTime_H

/* MinGW only has a snippet time.h not protected against multiple inclusion */
#if defined(__struct_timespec_defined)
#define _TIMESPEC_DEFINED 1
#endif

#if ! defined(_MINGW) || ! defined(_TIMESPEC_DEFINED)
#  if _MSC_VER >= 1900
#    include <time.h>
#  else

#define __struct_timespec_defined 1
#define _TIMESPEC_DEFINED 1
struct timespec {
    time_t tv_sec; /* seconds since some epoch */
    long tv_nsec; /* nanoseconds within the second */
};

#  endif /* _MSC_VER */
#endif /* ! defined(_MINGW) || ! defined(_TIMESPEC_DEFINED) */

#endif /* ifndef INC_osdTime_H */

