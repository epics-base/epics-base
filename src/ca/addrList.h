

#include "envDefs.h"  

#ifdef __cplusplus
extern "C" {
#endif

#if 0

void caSetupAddrList(
        ELLLIST *pList,
        SOCKET socket);
void caPrintAddrList(ELLLIST *pList);

epicsShareFunc void epicsShareAPI caDiscoverInterfaces(
	ELLLIST *pList,
	SOCKET socket,
	unsigned short port,
	struct in_addr matchAddr);

epicsShareFunc void caAddConfiguredAddr (
        cac *pcac, 
        ELLLIST *pList,
        const ENV_PARAM *pEnv,
        SOCKET socket,
        unsigned short port);

int local_addr(SOCKET socket, struct sockaddr_in *plcladdr);

epicsShareFunc unsigned short epicsShareAPI 
	caFetchPortConfig(const ENV_PARAM *pEnv, unsigned short defaultPort);

#endif

#ifdef __cplusplus
}
#endif

