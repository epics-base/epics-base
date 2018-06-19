/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef addrListh
#define addrListh

#include "shareLib.h"
#include "envDefs.h" 
#include "osiSock.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void epicsShareAPI configureChannelAccessAddressList 
    ( struct ELLLIST *pList, SOCKET sock, unsigned short port );

epicsShareFunc int epicsShareAPI addAddrToChannelAccessAddressList
    ( struct ELLLIST *pList, const ENV_PARAM *pEnv, 
    unsigned short port, int ignoreNonDefaultPort );

epicsShareFunc void epicsShareAPI printChannelAccessAddressList 
    ( const struct ELLLIST *pList );

epicsShareFunc void epicsShareAPI removeDuplicateAddresses
    ( struct ELLLIST *pDestList, ELLLIST *pSrcList, int silent);

#ifdef __cplusplus
}
#endif

#endif /* ifndef addrListh */

