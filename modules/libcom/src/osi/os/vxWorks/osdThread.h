/*************************************************************************\
* Copyright (c) 2021 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_osdThread_H
#define INC_osdThread_H

#include <epicsVersion.h>

#ifdef _WRS_VXWORKS_MAJOR
#  define VXWORKS_VERSION_INT VERSION_INT(_WRS_VXWORKS_MAJOR, \
        _WRS_VXWORKS_MINOR, _WRS_VXWORKS_MAINT, _WRS_VXWORKS_SVCPK)
#else
/* Version not available at compile-time, assume... */
#  define VXWORKS_VERSION_INT VERSION_INT(5, 5, 0, 0)
#endif

#if VXWORKS_VERSION_INT < VERSION_INT(6, 9, 4, 1)
/* VxWorks 6.9.4.1 and later can support joining threads */
#  undef EPICS_THREAD_CAN_JOIN
#endif

#endif /* INC_osdThread_H */
