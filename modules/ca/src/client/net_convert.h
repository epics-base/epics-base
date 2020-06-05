/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *
 *  Author: J. Hill
 *
 */

#ifndef INC_net_convert_H
#define INC_net_convert_H

#include "db_access.h"
#include "libCaAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long arrayElementCount;

LIBCA_API int caNetConvert (
    unsigned type, const void *pSrc, void *pDest,
    int hton, arrayElementCount count );

#ifdef __cplusplus
}
#endif

#endif	/* ifndef INC_net_convert_H */
