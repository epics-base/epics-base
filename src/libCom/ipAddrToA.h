
#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc void epicsShareAPI
	ipAddrToA(const struct sockaddr_in *pInetAddr, 
		char *pBuf, const unsigned bufSize);

#ifdef __cplusplus
}
#endif

