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
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   5-88
 */

#ifndef rsrvh
#define rsrvh

#include <stddef.h>
#include "dbCoreAPI.h"

#define RSRV_OK 0
#define RSRV_ERROR (-1)

#ifdef __cplusplus
extern "C" {
#endif

DBCORE_API void rsrv_register_server(void);

DBCORE_API void casr (unsigned level);
DBCORE_API int casClientInitiatingCurrentThread (
                        char * pBuf, size_t bufSize );
DBCORE_API void casStatsFetch (
                        unsigned *pChanCount, unsigned *pConnCount );

#ifdef __cplusplus
}
#endif

#endif /*rsrvh */
