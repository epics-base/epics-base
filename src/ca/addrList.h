
#ifdef __cplusplus
extern "C" {
#endif

#include "envDefs.h"  

void caSetupAddrList(
        ELLLIST *pList,
        SOCKET socket);

void caPrintAddrList(ELLLIST *pList);

epicsShareFunc void epicsShareAPI caDiscoverInterfaces(
	ELLLIST *pList,
	SOCKET socket,
	unsigned short port,
	struct in_addr matchAddr);

epicsShareFunc void epicsShareAPI caAddConfiguredAddr(
        ELLLIST *pList,
        const ENV_PARAM *pEnv,
        SOCKET socket,
        unsigned short port);

int local_addr(SOCKET socket, struct sockaddr_in *plcladdr);

epicsShareFunc unsigned short epicsShareAPI 
	caFetchPortConfig(const ENV_PARAM *pEnv, unsigned short defaultPort);

typedef union ca_addr {
        struct sockaddr_in      in;
        struct sockaddr         sa;
}caAddr;

typedef struct {
        ELLNODE		node;
        caAddr		srcAddr;
        caAddr		destAddr;
}caAddrNode;

#ifdef __cplusplus
}
#endif

