
#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc void epicsShareAPI
	ipAddrToA(const struct sockaddr_in *pInetAddr, 
		char *pBuf, const unsigned bufSize);

epicsShareFunc int epicsShareAPI 
	aToIPAddr(const char *pAddrString, unsigned short defaultPort, struct sockaddr_in *pIP);

#ifdef __cplusplus
}
#endif

