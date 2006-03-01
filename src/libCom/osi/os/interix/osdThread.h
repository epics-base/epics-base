/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef osdThreadh
#define osdThreadh

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define nanosleep(x, y) usleep( ((x)->tv_sec)*1000000 + ((y)->tv_nsec)/1000 )

pthread_t epicsThreadGetPosixThreadId ( epicsThreadId id );

#ifdef __cplusplus
}
#endif

#endif /* osdThreadh */
