//
// $Id$
//
// Author Jeff Hill
//
//
//
// $Log$
// Revision 1.5  1998/05/29 20:08:21  jhill
// use new sock ioctl() typedef
//
// Revision 1.4  1998/02/05 23:11:16  jhill
// use osiSock macros
//
// Revision 1.3  1997/06/13 09:16:15  jhill
// connect proto changes
//
// Revision 1.2  1997/04/10 19:40:33  jhill
// API changes
//
// Revision 1.1  1996/11/02 01:01:41  jhill
// installed
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
casIntfIO::casIntfIO() : 
	pNormalUDP(NULL),
	pBCastUDP(NULL),
	sock(INVALID_SOCKET)
{
	memset(&addr, '\0', sizeof(addr));
}

//
// casIntfIO::init()
//
caStatus casIntfIO::init(const caNetAddr &addrIn, casDGClient &dgClientIn,
		int autoBeaconAddr, int addConfigBeaconAddr)
{
	int yes = TRUE;
	int status;
	caStatus stat;
	int addrSize;

	if (!bsdSockAttach()) {
		return S_cas_internal;
	}

	/*
	 * Setup the server socket
	 */
	this->sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (this->sock==INVALID_SOCKET) {
		printf("No socket error was %s\n", SOCKERRSTR);
		return S_cas_noFD;
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
			SOCKERRSTR);
		return S_cas_internal;
	}

	this->addr = addrIn.getSockIP();
	status = bind(this->sock,(sockaddr *) &this->addr,
					sizeof(this->addr));
	if (status<0) {
		if (SOCKERRNO == SOCK_EADDRINUSE) {
			//
			// force assignement of a default port
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
			errPrintf(S_cas_bindFail,
				__FILE__, __LINE__,
				"- bind TCP IP addr=%s port=%u failed because %s",
				inet_ntoa(this->addr.sin_addr),
				ntohs(this->addr.sin_port),
				SOCKERRSTR);
			return S_cas_bindFail;
		}
	}

	addrSize = sizeof(this->addr);
	status = getsockname(this->sock, 
			(struct sockaddr *)&this->addr, &addrSize);
	if (status) {
		ca_printf("CAS: getsockname() error %s\n", 
			SOCKERRSTR);
		return S_cas_internal;
	}

	//
	// be sure of this now so that we can fetch the IP 
	// address and port number later
	//
    assert (this->addr.sin_family == AF_INET);

    status = listen(this->sock, caServerConnectPendQueueSize);
    if(status < 0) {
		ca_printf("CAS: listen() error %s\n", SOCKERRSTR);
		return S_cas_internal;
    }

	//
	// set up a DG socket bound to the specified interface
	// (or INADDR_ANY)
	//
	this->pNormalUDP = this->newDGIntfIO(dgClientIn);
	if (!this->pNormalUDP) {
		return S_cas_noMemory;
	}
	stat = this->pNormalUDP->init(this->addr, this->portNumber(), 
			autoBeaconAddr, addConfigBeaconAddr);
	if (stat) {
		return stat;
	}
	else {
		this->pNormalUDP->start();
	}

	//
	// If they are binding to a particular interface then
	// we will also need to bind to the broadcast address 
	// for that interface (if it has one)
	//
	if (this->addr.sin_addr.s_addr != htonl(INADDR_ANY)) {
		this->pBCastUDP = this->newDGIntfIO(dgClientIn);
		if (this->pBCastUDP) {
			stat = this->pBCastUDP->init(addr, this->portNumber(), 
					FALSE, FALSE, TRUE, this->pNormalUDP);
			if (stat) {
				if (stat==S_cas_noInterface) {
					delete this->pBCastUDP;
					this->pBCastUDP = NULL;
				}
				else {
					errMessage(stat, 
					"server will not receive broadcasts");
				}
			}
			else {
				this->pBCastUDP->start();
			}
		}
		else {
			errMessage(S_cas_noMemory,
			"failed to create broadcast socket");
		}
	}
	return S_cas_success;
}

//
// casIntfIO::~casIntfIO()
//
casIntfIO::~casIntfIO()
{
	if (this->sock != INVALID_SOCKET) {
		socket_close(this->sock);
	}
	if (this->pNormalUDP) {
		delete this->pNormalUDP;
	}
	if (this->pBCastUDP) {
		delete this->pBCastUDP;
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
                if (SOCKERRNO!=SOCK_EWOULDBLOCK) {
                        ca_printf(
                                "CAS: %s accept error %s\n",
                                __FILE__,
                                SOCKERRSTR);
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
                                __FILE__, SOCKERRSTR);
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

//
// casIntfIO::requestBeacon()
//
void casIntfIO::requestBeacon()
{
	//
	// the broadcast bound socket is not used here because
	// it will have the wrong source address. This
	// casDGIntfIO has a list of all beacon addresses
	// that have been configured (no need to use
	// this->pBCastUDP).
	//
	if (this->pNormalUDP) {
		casDGClient::sendBeacon(*this->pNormalUDP);
	}
}

