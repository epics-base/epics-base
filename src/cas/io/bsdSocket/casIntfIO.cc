//
// $Id$
//
// Author Jeff Hill
//
//
//
//

#include "server.h"
#include "bsdSocketResource.h"

//
// 5 appears to be a TCP/IP built in maximum
//
const unsigned caServerConnectPendQueueSize = 5u;

//
// casIntfIO::casIntfIO()
//
casIntfIO::casIntfIO (const caNetAddr &addrIn) : 
	sock (INVALID_SOCKET),
    addr (addrIn.getSockIP())
{
	int yes = TRUE;
	int status;
	int addrSize;

	if (!bsdSockAttach()) {
		throw S_cas_internal;
	}

	/*
	 * Setup the server socket
	 */
	this->sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (this->sock==INVALID_SOCKET) {
		printf("No socket error was %s\n", SOCKERRSTR(SOCKERRNO));
		throw S_cas_noFD;
	}

	/*
	 * release the port in case we exit early
	 */
	status = setsockopt (
					this->sock,
					SOL_SOCKET,
					SO_REUSEADDR,
					(char *) &yes,
					sizeof (yes));
	if (status<0) {
		ca_printf("CAS: server set SO_REUSEADDR failed? %s\n",
			SOCKERRSTR(SOCKERRNO));
        socket_close (this->sock);
		throw S_cas_internal;
	}

	status = bind(this->sock,(sockaddr *) &this->addr,
					sizeof(this->addr));
	if (status<0) {
		if (SOCKERRNO == SOCK_EADDRINUSE) {
			//
			// force assignment of a default port
			// (so the getsockname() call below will
			// work correctly)
			//
			this->addr.sin_port = ntohs (0);
			status = bind(
				this->sock,
				(sockaddr *)&this->addr,
				sizeof(this->addr));
		}
		if (status<0) {
			char buf[64];
            int errnoCpy = SOCKERRNO;

			ipAddrToA (&this->addr, buf, sizeof(buf));
			errPrintf(S_cas_bindFail,
				__FILE__, __LINE__,
				"- bind TCP IP addr=%s failed because %s",
				buf, SOCKERRSTR(errnoCpy));
            socket_close (this->sock);
			throw S_cas_bindFail;
		}
	}

	addrSize = sizeof (this->addr);
	status = getsockname (this->sock, 
			(struct sockaddr *)&this->addr, &addrSize);
	if (status) {
		ca_printf("CAS: getsockname() error %s\n", 
			SOCKERRSTR(SOCKERRNO));
        socket_close (this->sock);
		throw S_cas_internal;
	}

	//
	// be sure of this now so that we can fetch the IP 
	// address and port number later
	//
    assert (this->addr.sin_family == AF_INET);

    status = listen(this->sock, caServerConnectPendQueueSize);
    if(status < 0) {
		ca_printf("CAS: listen() error %s\n", SOCKERRSTR(SOCKERRNO));
        socket_close (this->sock);
		throw S_cas_internal;
    }
}

//
// casIntfIO::~casIntfIO()
//
casIntfIO::~casIntfIO()
{
	if (this->sock != INVALID_SOCKET) {
		socket_close(this->sock);
	}

	bsdSockRelease();
}

//
// newStreamIO::newStreamClient()
//
casStreamOS *casIntfIO::newStreamClient(caServerI &cas) const
{
    struct sockaddr	newAddr;
    SOCKET          newSock;
    int             length;
    casStreamOS	*pOS;
    
    length = sizeof(newAddr);
    newSock = accept(this->sock, &newAddr, &length);
    if (newSock==INVALID_SOCKET) {
        int errnoCpy = SOCKERRNO;
        if (errnoCpy!=SOCK_EWOULDBLOCK) {
            ca_printf ("CAS: %s accept error %s\n",
                __FILE__,SOCKERRSTR(errnoCpy));
        }
        return NULL;
    }
    else if (sizeof(newAddr)>(size_t)length) {
        socket_close(newSock);
        ca_printf("CAS: accept returned bad address len?\n");
        return NULL;
    }
    
    ioArgsToNewStreamIO args;
    args.addr = newAddr;
    args.sock = newSock;
    pOS = new casStreamOS(cas, args);
    if (!pOS) {
        socket_close(newSock);
    }
    else {
        if ( cas.getDebugLevel()>0u) {
            char pName[64u];
            
            pOS->clientHostName (pName, sizeof (pName));
            ca_printf("CAS: allocated client object for \"%s\"\n", pName);
        }
    }
    return pOS;
}

//
// casIntfIO::setNonBlocking()
//
void casIntfIO::setNonBlocking()
{
        int status;
        osiSockIoctl_t yes = TRUE;
 
        status = socket_ioctl(this->sock, FIONBIO, &yes);
        if (status<0) {
                ca_printf(
                "%s:CAS: server non blocking IO set fail because \"%s\"\n",
                                __FILE__, SOCKERRSTR(SOCKERRNO));
        }
}
 
//
// casIntfIO::getFD()
//
int casIntfIO::getFD() const
{
        return this->sock;
}

//
// casIntfIO::show()
//
void casIntfIO::show(unsigned level) const
{
	if (level>2u) {
		printf(" casIntfIO::sock = %d\n", this->sock);
	}
}

//
// casIntfIO::portNumber()
//
unsigned casIntfIO::portNumber() const
{
	return ntohs(this->addr.sin_port);
}


