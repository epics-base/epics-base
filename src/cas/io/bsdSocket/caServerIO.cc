//
// $Id$
//
// verify connection state prior to doing anything in this file
//
//

#include <ctype.h>

#include "server.h"
#include "sigPipeIgnore.h"
#include "addrList.h"
#include "bsdSocketResource.h"

static char *getToken(const char **ppString, char *pBuf, unsigned bufSIze);

int caServerIO::staticInitialized;

//
// caServerIO::caServerIO()
//
caServerIO::caServerIO ()
{
	if (!bsdSockAttach()) {
		throw S_cas_internal;
	}

	caServerIO::staticInit();
}

//
// caServerIO::locateInterfaces()
//
void caServerIO::locateInterfaces ()
{
	char buf[64u]; 
	const char *pStr; 
	char *pToken;
	caStatus stat;
	unsigned short port;
	struct sockaddr_in saddr;
	bool autoBeaconAddr;

	//
	// first try for the server's private port number env var.
	// If this available use the CA server port number (used by
	// clients to find the server). If this also isnt available
	// then use a hard coded default - CA_SERVER_PORT.
	//
	if (envGetConfigParamPtr(&EPICS_CAS_SERVER_PORT)) {
		port = caFetchPortConfig(&EPICS_CAS_SERVER_PORT, CA_SERVER_PORT);
	}
	else {
		port = caFetchPortConfig(&EPICS_CA_SERVER_PORT, CA_SERVER_PORT);
	}

	memset ((char *)&saddr,0,sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port =  ntohs (port);

	pStr = envGetConfigParam(&EPICS_CA_AUTO_ADDR_LIST, sizeof(buf), buf);
	if (pStr) {
		if (strstr(pStr,"no")||strstr(pStr,"NO")) {
			autoBeaconAddr = false;
		}
		else if (strstr(pStr,"yes")||strstr(pStr,"YES")) {
			autoBeaconAddr = true;
		}
		else {
			fprintf(stderr, 
		"CAS: EPICS_CA_AUTO_ADDR_LIST = \"%s\"? Assuming \"YES\"\n", pStr);
			autoBeaconAddr = true;
		}
	}
	else {
		autoBeaconAddr = true;
	}

	//
	// bind to the the interfaces specified - otherwise wildcard
	// with INADDR_ANY and allow clients to attach from any interface
	//
	pStr = envGetConfigParamPtr(&EPICS_CAS_INTF_ADDR_LIST);
	if (pStr) {
		bool configAddrOnceFlag = true;
		stat = S_cas_noInterface; 
		while ( (pToken = getToken(&pStr, buf, sizeof(buf))) ) {
			int status;

			status = aToIPAddr (pToken, 0u, &saddr);
			if (status) {
				ca_printf(
					"%s: Parsing '%s'\n",
					__FILE__,
					EPICS_CAS_INTF_ADDR_LIST.name);
				ca_printf(
					"\tBad internet address or host name: '%s'\n",
					pToken);
				continue;
			}
			stat = this->attachInterface (caNetAddr(saddr), autoBeaconAddr, configAddrOnceFlag);
			if (stat) {
				errMessage(stat, NULL);
				break;
			}
			configAddrOnceFlag = false;
		}
	}
	else {
		saddr.sin_addr.s_addr = htonl(INADDR_ANY);
		stat = this->attachInterface (caNetAddr(saddr), autoBeaconAddr, true);
		if (stat) {
			errMessage(stat, NULL);
		}
	}
}

//
// caServerIO::~caServerIO()
//
caServerIO::~caServerIO()
{
	bsdSockRelease();
}

//
// caServerIO::staticInit()
//
inline void caServerIO::staticInit()
{
	if (caServerIO::staticInitialized) {
		return;
	}

	installSigPipeIgnore();

	caServerIO::staticInitialized = TRUE;
}

//
// caServerIO::show()
//
void caServerIO::show (unsigned /* level */) const
{
	printf ("caServerIO at %p\n", this);
}

//
// getToken()
//
static char *getToken(const char **ppString, char *pBuf, unsigned bufSIze)
{
    const char *pToken;
    unsigned i;
    
    pToken = *ppString;
    while(isspace(*pToken)&&*pToken){
        pToken++;
    }
    
    for (i=0u; i<bufSIze; i++) {
        if (isspace(pToken[i]) || pToken[i]=='\0') {
            pBuf[i] = '\0';
            break;
        }
        pBuf[i] = pToken[i];
    }
    
    *ppString = &pToken[i];
    
    if(*pToken){
        return pBuf;
    }
    else{
        return NULL;
    }
}

