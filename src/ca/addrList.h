
#include <envDefs.h>

void caSetupAddrList(
        ELLLIST *pList,
        SOCKET socket);

void caPrintAddrList();

void caDiscoverInterfaces(
        ELLLIST *pList,
        SOCKET socket,
        int port);

void caAddConfiguredAddr(
        ELLLIST *pList,
        ENV_PARAM *pEnv,
        SOCKET socket,
        int port);

int local_addr(SOCKET socket, struct sockaddr_in *plcladdr);
int caFetchPortConfig(ENV_PARAM *pEnv, int defaultPort);

union caAddr{
        struct sockaddr_in      inetAddr;
        struct sockaddr         sockAddr;
};

typedef struct {
        ELLNODE                 node;
        union caAddr            srcAddr;
        union caAddr            destAddr;
}caAddrNode;


