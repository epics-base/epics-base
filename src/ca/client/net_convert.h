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
 *
 *	Author: J. Hill 
 *
 */

#ifndef _NET_CONVERT_H
#define _NET_CONVERT_H

#include "db_access.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long arrayElementCount;

epicsShareFunc int caNetConvert ( 
    unsigned type, const void *pSrc, void *pDest, 
    int hton, arrayElementCount count );

#ifdef __cplusplus
}
#endif

#endif	/* define _NET_CONVERT_H */
