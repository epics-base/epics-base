/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
// verify connection state prior to doing anything in this file
//

#include <ctype.h>
#include <list>

#include "epicsSignal.h"
#include "envDefs.h"
#include "caProto.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#include "caServerIO.h"

static char * getToken ( const char **ppString, char *pBuf, unsigned bufSIze );

int caServerIO::staticInitialized;

//
// caServerIO::caServerIO()
//
caServerIO::caServerIO ()
{
	if ( ! osiSockAttach () ) {
		throw S_cas_internal;
	}

	caServerIO::staticInit ();
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
	if ( envGetConfigParamPtr ( & EPICS_CAS_SERVER_PORT ) ) {
		port = envGetInetPortConfigParam (
                    &EPICS_CAS_SERVER_PORT,
                    static_cast <unsigned short> ( CA_SERVER_PORT ) );
	}
	else {
		port = envGetInetPortConfigParam (
                    & EPICS_CA_SERVER_PORT,
                    static_cast <unsigned short> ( CA_SERVER_PORT ) );
	}

	memset ((char *)&saddr,0,sizeof(saddr));

    pStr = envGetConfigParam ( &EPICS_CAS_AUTO_BEACON_ADDR_LIST, sizeof(buf), buf );
    if ( ! pStr ) {
	    pStr = envGetConfigParam ( &EPICS_CA_AUTO_ADDR_LIST, sizeof(buf), buf );
    }
	if (pStr) {
		if (strstr(pStr,"no")||strstr(pStr,"NO")) {
			autoBeaconAddr = false;
		}
		else if (strstr(pStr,"yes")||strstr(pStr,"YES")) {
			autoBeaconAddr = true;
		}
		else {
			fprintf(stderr, 
		"CAS: EPICS_CA(S)_AUTO_ADDR_LIST = \"%s\"? Assuming \"YES\"\n", pStr);
			autoBeaconAddr = true;
		}
	}
	else {
		autoBeaconAddr = true;
	}

    typedef std::list<osiSockAddr> mcastAddrs_t;
    mcastAddrs_t mcastAddrs;

	//
	// bind to the the interfaces specified - otherwise wildcard
	// with INADDR_ANY and allow clients to attach from any interface
	//
	pStr = envGetConfigParamPtr ( & EPICS_CAS_INTF_ADDR_LIST );
	if (pStr) {
		bool configAddrOnceFlag = true;
		stat = S_cas_noInterface; 
		while ( (pToken = getToken ( & pStr, buf, sizeof ( buf ) ) ) ) {
			int status;

			status = aToIPAddr (pToken, port, &saddr);
			if (status) {
				errlogPrintf(
					"%s: Parsing '%s'\n",
					__FILE__,
					EPICS_CAS_INTF_ADDR_LIST.name);
				errlogPrintf(
					"\tBad internet address or host name: '%s'\n",
					pToken);
				continue;
			}

            epicsUInt32 top = ntohl(saddr.sin_addr.s_addr)>>24;
            if (saddr.sin_family==AF_INET && top>=224 && top<=239) {
                osiSockAddr oaddr;
                oaddr.ia = saddr;
                mcastAddrs.push_back(oaddr);
                continue;
            }

			stat = this->attachInterface (caNetAddr(saddr), autoBeaconAddr, configAddrOnceFlag);
			if (stat) {
				errMessage(stat, "unable to attach explicit interface");
				break;
			}
			configAddrOnceFlag = false;
		}
	}
	else {
        saddr.sin_family = AF_INET;
        saddr.sin_port =  ntohs (port);
		saddr.sin_addr.s_addr = htonl(INADDR_ANY);
		stat = this->attachInterface (caNetAddr(saddr), autoBeaconAddr, true);
		if (stat) {
			errMessage(stat, "unable to attach any interface");
		}
	}

    for (mcastAddrs_t::const_iterator it = mcastAddrs.begin(); it!=mcastAddrs.end(); ++it) {
        this->addMCast(*it);
    }
}

//
// caServerIO::~caServerIO()
//
caServerIO::~caServerIO()
{
	osiSockRelease();
}

//
// caServerIO::staticInit()
//
inline void caServerIO::staticInit()
{
	if ( caServerIO::staticInitialized ) {
		return;
	}

	epicsSignalInstallSigPipeIgnore ();

	caServerIO::staticInitialized = true;
}

//
// caServerIO::show()
//
void caServerIO::show (unsigned /* level */) const
{
	printf ( "caServerIO at %p\n", 
        static_cast <const void *> ( this ) );
}

//
// getToken()
//
static char *getToken(const char **ppString,
                      char *pBuf, unsigned bufSIze)
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

