/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef osdThreadh
#define osdThreadh

/* VxWorks 6.9 and later can support joining threads */

#if (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR < 9)
#undef EPICS_THREAD_CAN_JOIN
#endif

#endif /* osdThreadh */
