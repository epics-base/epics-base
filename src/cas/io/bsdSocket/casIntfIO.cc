//
// $Id$
//
// Author Jeff Hill
//
//
//
// $Log$
// Revision 1.1  1996/11/02 01:01:41  jhill
// installed
//
//

#include "server.h"

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
caStatus casIntfIO::init(const caAddr &addrIn, casDGClient &dgClientIn,
		int autoBeaconAddr, int addConfigBeaconAddr)
{
	int		yes = TRUE;
	int		status;
	caStatus	stat;
	int		addrSize;

        /*
         * Setup the server socket
         */
        this->sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (this->sock==INVALID_SOCKET) {
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
                ca_printf("CAS: server set SO_REUSEADDR failed?\n",
			strerror(SOCKERRNO));
		return S_cas_internal;
        }

	this->addr = addrIn;
        status = bind(
                        this->sock,
                        (sockaddr *) &this->addr.sa,
                        sizeof(this->addr));
        if (status<0) {
                if (SOCKERRNO == EADDRINUSE) {
			//
			// force assignement of a default port
			// (so the getsockname() call below will
			// work correctly)
			//
        		this->addr.in.sin_port = ntohs (0);
			status = bind(
					this->sock,
					&this->addr.sa,
					sizeof(this->addr));
		}
		if (status<0) {
			errPrintf(S_cas_bindFail,
				__FILE__, __LINE__,
				"- bind TCP IP addr=%s port=%u failed because %s",
				inet_ntoa(this->addr.in.sin_addr),
				ntohs(this->addr.in.sin_port),
				strerror(SOCKERRNO));
			return S_cas_bindFail;
		}
        }

	addrSize = sizeof(this->addr);
	status = getsockname(this->sock, &this->addr.sa, &addrSize);
	if (status) {
                ca_printf("CAS: getsockname() error %s\n", strerror(SOCKERRNO));
		return S_cas_internal;
	}

	//
	// be sure of this now so that we can fetch the IP 
	// address and port number later
	//
        assert (this->addr.sa.sa_family == AF_INET);

        status = listen(this->sock, caServerConnectPendQueueSize);
        if(status < 0) {
                ca_printf("CAS: listen() error %s\n", strerror(SOCKERRNO));
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
	stat = this->pNormalUDP->init(addr, this->portNumber(), 
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
	if (this->addr.in.sin_addr.s_addr != INADDR_ANY) {
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
}

//
// newStreamIO::newStreamClient()
//
casStreamOS *casIntfIO::newStreamClient(caServerI &cas) const
{
        caAddr          newAddr;
        SOCKET          newSock;
        int             length;
	casStreamOS	*pOS;
 
        length = sizeof(newAddr.sa);
        newSock = accept(this->sock, &newAddr.sa, &length);
        if (newSock==INVALID_SOCKET) {
                if (SOCKERRNO!=EWOULDBLOCK) {
                        ca_printf(
                                "CAS: %s accept error %s\n",
                                __FILE__,
                                strerror(SOCKERRNO));
                }
                return NULL;
        }
        else if (sizeof(newAddr.sa)>(size_t)length) {
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
        int yes = TRUE;
 
        status = socket_ioctl(this->sock, FIONBIO, &yes);
        if (status<0) {
                ca_printf(
                "%s:CAS: server non blocking IO set fail because \"%s\"\n",
                                __FILE__, strerror(SOCKERRNO));
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
	return ntohs(this->addr.in.sin_port);
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

