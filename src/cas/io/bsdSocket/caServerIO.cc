//
// $Id$
//
// verify connection state prior to doing anything in this file
//
//
// $Log$
//

#include <server.h>
#include <sigPipeIgnore.h>

const unsigned caServerConnectPendQueueSize = 10u;

int caServerIO::staticInitialized;

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
caStatus caServerIO::init()
{
	caAddr	serverAddr;
	int	yes = TRUE;
	int	status;
	int	len;
	int	port;

	caServerIO::staticInit();

        /*
         * Setup the server socket
         */
        this->sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (this->sock<0) {
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

        memset ((char *)&serverAddr, '\0', sizeof(serverAddr));
        serverAddr.sa.sa_family = AF_INET;
        serverAddr.in.sin_addr.s_addr = INADDR_ANY;
	port = caFetchPortConfig(&EPICS_CA_SERVER_PORT, CA_SERVER_PORT);
        serverAddr.in.sin_port = ntohs (port);
        status = bind(
                        this->sock,
                        &serverAddr.sa,
                        sizeof(serverAddr.sa));
        if (status<0) {
                if (SOCKERRNO == EADDRINUSE) {
			//
			// force assignement of a default port
			// (so the getsockname() call below will
			// work correctly)
			//
        		serverAddr.in.sin_port = ntohs (0);
			status = bind(
					this->sock,
					&serverAddr.sa,
					sizeof(serverAddr.sa));
			if (status<0) {
                        	ca_printf (
			"CAS: default bind failed because \"%s\"\n",
                       		         strerror(SOCKERRNO));
				return S_cas_portInUse;
			}
		}
		else {
                        ca_printf("CAS: bind failed because \"%s\"\n",
                                strerror(SOCKERRNO));
			return S_cas_portInUse;
                }
        }

        len = sizeof (this->addr.sa);
        status = getsockname (
                        this->sock,
                        &this->addr.sa,
                        &len);
        if (status<0) {
                ca_printf("CAS: getsockname failed because: %s\n",
                        strerror(SOCKERRNO));
                return S_cas_internal;
        }

	//
	// be sure of this now so that we can fetch the IP 
	// address and port number later
	//
        assert (this->addr.sa.sa_family == AF_INET);

        status = listen(this->sock, caServerConnectPendQueueSize);
        if(status < 0) {
                ca_printf("CAS: Listen error %s\n",strerror(SOCKERRNO));
                return S_cas_internal;
        }
	this->sockState = casOnLine;
	return S_cas_success;
}

//
// caServerIO::serverPortNumber()
//
unsigned caServerIO::serverPortNumber() const
{
	return (unsigned) ntohs(this->addr.in.sin_port);
}

//
// caServerIO::show()
//
void caServerIO::show (unsigned level)
{
	printf ("caServerIO at %x\n", (unsigned) this);
	if (level>1u) {
		printf ("\tsock = %d, state = %d\n", 
			this->sock, this->sockState);
	}
}

//
// caServerIO::newDGIO()
//
casMsgIO *caServerIO::newDGIO() const
{
	casDGIO		*pDG;

	pDG = new casDGIO();
	if (!pDG) {
		return pDG;
	}

	return pDG;
}

//
// caServerIO::newStreamIO()
//
casMsgIO *caServerIO::newStreamIO() const
{
	caAddr 		newAddr;
	SOCKET  	newSock;
        int     	length;

        length = sizeof(newAddr.sa);
        newSock = accept(this->sock, &newAddr.sa, &length);
        if (newSock<0) {
                if (SOCKERRNO!=EWOULDBLOCK) {
                        ca_printf(
                                "CAS: %s accept error %s\n",
                                __FILE__,
                                strerror(SOCKERRNO));
                }
                return NULL;
        }
	else if (sizeof(newAddr.sa)>(size_t)length) {
		ca_printf("CAS: accept returned bad address len?\n");
		return NULL;
	}

	return new casStreamIO(newSock, newAddr);
}

//
// caServerIO::setNonBlocking()
//
void caServerIO::setNonBlocking()
{
	int status;
	int yes = TRUE;

	status = socket_ioctl(this->sock, FIONBIO, &yes);
	if (status<0) {
		ca_printf(
		"%s:CAS: server non blocking IO set fail because \"%s\"\n", 
				__FILE__, strerror(SOCKERRNO));
		this->sockState = casOffLine;
	}
}

//
// caServerIO::getFD()
//
int caServerIO::getFD() const
{
	return this->sock;
}


