
#ifndef addrListh
#define addrListh

#include "shareLib.h"
#include "envDefs.h"  

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void epicsShareAPI configureChannelAccessAddressList 
    ( ELLLIST *pList, SOCKET sock, unsigned short port );

epicsShareFunc void epicsShareAPI addAddrToChannelAccessAddressList 
    ( ELLLIST *pList, const ENV_PARAM *pEnv, unsigned short port );

epicsShareFunc void epicsShareAPI printChannelAccessAddressList 
    ( const ELLLIST *pList );

epicsShareFunc void epicsShareAPI removeDuplicateAddresses
    ( ELLLIST *pDestList, ELLLIST *pSrcList, int silent);

#ifdef __cplusplus
}
#endif

#endif /* ifndef addrListh */

