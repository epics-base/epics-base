//
// $Id$
//
// verify connection state prior to doing anything in this file
//
//
// $Log$
// Revision 1.3  1996/11/02 00:54:42  jhill
// many improvements
//
// Revision 1.2  1996/06/21 02:12:40  jhill
// SOLARIS port
//
// Revision 1.1.1.1  1996/06/20 00:28:19  jhill
// ca server installation
//
//

#include <ctype.h>

#include "server.h"
#include "sigPipeIgnore.h"

static char *getToken(const char **ppString, char *pBuf, unsigned bufSIze);

int caServerIO::staticInitialized;

//
// caServerIO::~caServerIO()
//
caServerIO::~caServerIO()
{
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
// caServerIO::init()
//
caStatus caServerIO::init(caServerI &cas)
{
	char buf[64u];
	const char *pStr;
	char *pToken;
	caStatus stat;
	unsigned short port;
	caAddr addr;
	int autoBeaconAddr;

	caServerIO::staticInit();

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

	memset((char *)&addr,0,sizeof(addr));
	addr.sa.sa_family = AF_INET;
	addr.in.sin_port =  ntohs (port);

	pStr = envGetConfigParam(&EPICS_CA_AUTO_ADDR_LIST, sizeof(buf), buf);
	if (pStr) {
		if (strstr(pStr,"no")||strstr(pStr,"NO")) {
			autoBeaconAddr = FALSE;
		}
		else if (strstr(pStr,"yes")||strstr(pStr,"YES")) {
			autoBeaconAddr = TRUE;
		}
		else {
			fprintf(stderr, 
		"CAS: EPICS_CA_AUTO_ADDR_LIST = \"%s\"? Assuming \"YES\"\n", pStr);
			autoBeaconAddr = TRUE;
		}
	}
	else {
		autoBeaconAddr = TRUE;
	}

	//
	// bind to the the interfaces specified - otherwise wildcard
	// with INADDR_ANY and allow clients to attach from any interface
	//
	pStr = envGetConfigParamPtr(&EPICS_CAS_INTF_ADDR_LIST);
	if (pStr) {
		int configAddrOnceFlag = TRUE;
		stat = S_cas_noInterface; 
		while ( (pToken = getToken(&pStr, buf, sizeof(buf))) ) {
			addr.in.sin_addr.s_addr = inet_addr(pToken);
			if (addr.in.sin_addr.s_addr == ~0ul) {
				ca_printf(
					"%s: Parsing '%s'\n",
					__FILE__,
					EPICS_CAS_INTF_ADDR_LIST.name);
				ca_printf(
					"\tBad internet address format: '%s'\n",
					pToken);
				continue;
			}
			stat = cas.addAddr(addr, autoBeaconAddr, configAddrOnceFlag);
			if (stat) {
				errMessage(stat, NULL);
				break;
			}
			configAddrOnceFlag = FALSE;
		}
	}
	else {
		addr.in.sin_addr.s_addr = INADDR_ANY;
		stat = cas.addAddr(addr, autoBeaconAddr, TRUE);
		if (stat) {
			errMessage(stat, NULL);
		}
	}

	return stat;
}

//
// caServerIO::show()
//
void caServerIO::show (unsigned /* level */) const
{
	printf ("caServerIO at %x\n", (unsigned) this);
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


