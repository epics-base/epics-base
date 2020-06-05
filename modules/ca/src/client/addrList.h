/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_addrList_H
#define INC_addrList_H

#include "envDefs.h"
#include "osiSock.h"

#include "libCaAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

LIBCA_API void epicsStdCall configureChannelAccessAddressList
    ( struct ELLLIST *pList, SOCKET sock, unsigned short port );

LIBCA_API int epicsStdCall addAddrToChannelAccessAddressList
    ( struct ELLLIST *pList, const ENV_PARAM *pEnv,
    unsigned short port, int ignoreNonDefaultPort );

LIBCA_API void epicsStdCall printChannelAccessAddressList
    ( const struct ELLLIST *pList );

LIBCA_API void epicsStdCall removeDuplicateAddresses
    ( struct ELLLIST *pDestList, ELLLIST *pSrcList, int silent);

#ifdef __cplusplus
}
#endif

#endif /* ifndef INC_addrList_H */

