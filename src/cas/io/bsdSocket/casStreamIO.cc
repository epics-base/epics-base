//
// $Id$
//
// verify connection state prior to doing anything in this file
//
//
// $Log$
// Revision 1.9  1997/04/10 19:40:34  jhill
// API changes
//
// Revision 1.8  1997/01/10 00:00:01  jhill
// close() => socket_close()
//
// Revision 1.7.2.1  1996/11/25 16:33:00  jhill
// close() => socket_close()
//
// Revision 1.7  1996/11/22 19:27:04  jhill
// suppressed error msg and returned correct # bytes pending
//
// Revision 1.6  1996/11/02 00:54:46  jhill
// many improvements
//
// Revision 1.5  1996/09/16 18:25:16  jhill
// vxWorks port changes
//
// Revision 1.4  1996/07/24 22:03:36  jhill
// fixed net proto for gnu compiler
//
// Revision 1.3  1996/07/09 22:55:22  jhill
// added cast
//
// Revision 1.2  1996/06/21 02:18:11  jhill
// SOLARIS port
//
// Revision 1.1.1.1  1996/06/20 00:28:19  jhill
// ca server installation
//
//

#include "server.h"


//
// casStreamIO::casStreamIO()
//
casStreamIO::casStreamIO(caServerI &cas, const ioArgsToNewStreamIO &args) :
	casStrmClient(cas),
	sockState(casOffLine), sock(args.sock), addr(args.addr),
	blockingFlag(xIsBlocking)
{
	assert (sock>=0);
}


//
// casStreamIO::init()
//
caStatus casStreamIO::init() 
{
	int 	yes = TRUE;
	int	status;
	int i;

        /*
         * see TCP(4P) this seems to make unsollicited single events much
         * faster. I take care of queue up as load increases.
         */
        status = setsockopt(
                                this->sock,
                                IPPROTO_TCP,
                                TCP_NODELAY,
                                (char *)&yes,
                                sizeof(yes));
        if (status<0) {
		ca_printf(
			"CAS: %s TCP_NODELAY option set failed %s\n",
			__FILE__, strerror(SOCKERRNO));
		return S_cas_internal;
        }

        /*
         * turn on KEEPALIVE so if the client crashes
         * this task will find out and exit
         */
        status = setsockopt(
                        sock,
                        SOL_SOCKET,
                        SO_KEEPALIVE,
                        (char *)&yes,
                        sizeof(yes));
        if (status<0) {
		ca_printf(
			"CAS: %s SO_KEEPALIVE option set failed %s\n",
			__FILE__, strerror(SOCKERRNO));
		return S_cas_internal;
        }


        /*
         * set TCP buffer sizes to be synergistic
         * with CA internal buffering
         */
        i = MAX_MSG_SIZE;
        status = setsockopt(
                        sock,
                        SOL_SOCKET,
                        SO_SNDBUF,
                        (char *)&i,
                        sizeof(i));
        if(status < 0){
                ca_printf("CAS: SO_SNDBUF set failed\n");
		return S_cas_internal;
        }
        i = MAX_MSG_SIZE;
        status = setsockopt(
                        sock,
                        SOL_SOCKET,
                        SO_RCVBUF,
                        (char *)&i,
                        sizeof(i));
        if(status < 0){
                ca_printf("CAS: SO_RCVBUF set failed\n");
		return S_cas_internal;
        }

	this->sockState = casOnLine;

	return this->casStrmClient::init();
}


//
// casStreamIO::~casStreamIO()
//
casStreamIO::~casStreamIO()
{
	if (sock>=0) {
		socket_close(this->sock);
	}
}


//
// casStreamIO::osdSend()
//
xSendStatus casStreamIO::osdSend(const char *pInBuf, bufSizeT nBytesReq, 
		bufSizeT &nBytesActual)
{
        int	status;

        if (this->sockState!=casOnLine) {
                return xSendDisconnect;
        }

        if (nBytesReq<=0u) {
		nBytesActual = 0u;
                return xSendOK;
        }

	status = send (
			this->sock,
			(char *) pInBuf,
			nBytesReq,
			0);
	if (status == 0) {
		this->sockState = casOffLine;
		return xSendDisconnect;
	}
	else if (status<0) {
		int anerrno = SOCKERRNO;

		if (anerrno != EWOULDBLOCK) {
			this->sockState = casOffLine;
		}
		nBytesActual = 0u;
		return xSendOK;
	}
	nBytesActual = (bufSizeT) status;
	return xSendOK;
}


//
// casStreamIO::osdRecv()
//
xRecvStatus casStreamIO::osdRecv(char *pInBuf, bufSizeT nBytes, 
		bufSizeT &nBytesActual)
{
  	int 		nchars;

	if (this->sockState!= casOnLine) {
		return xRecvDisconnect;
	}

	nchars = recv(this->sock, pInBuf, nBytes, 0);
	if (nchars==0) {
		this->sockState = casOffLine;
		return xRecvDisconnect;
	}
	else if (nchars<0) {
		char buf[64];

		/*
		 * normal conn lost conditions
		 */
		switch(SOCKERRNO){
		case EWOULDBLOCK:
			nBytesActual = 0u;
			return xRecvOK;

		case ECONNABORTED:
		case ECONNRESET:
		case ETIMEDOUT:
			break;

		default:
			ipAddrToA(&this->addr.in, buf, sizeof(buf));
			ca_printf(
		"CAS: client %s disconnected because \"%s\"\n",
				buf, strerror(SOCKERRNO));
			break;
		}
		this->sockState = casOffLine;
		return xRecvDisconnect;
	}
	nBytesActual = (bufSizeT) nchars;
	return xRecvOK;
}


//
// casStreamIO::show()
//
void casStreamIO::osdShow (unsigned level) const
{
	printf ("casStreamIO at %x\n", (unsigned) this);
	if (level>1u) {
		char buf[64];
		ipAddrToA(&this->addr.in, buf, sizeof(buf));
		printf (
			"client=%s, port=%x\n",
			buf, ntohs(this->addr.in.sin_port));
	}
}


//
// casStreamIO::xSsetNonBlocking()
//
void casStreamIO::xSetNonBlocking()
{
	int status;
	int yes = TRUE;

        if (this->sockState!=casOnLine) {
                return;
        }

	status = socket_ioctl(this->sock, FIONBIO, &yes);
	if (status>=0) {
		this->blockingFlag = xIsntBlocking;
	}
	else {
		ca_printf("%s:CAS: TCP non blocking IO set fail because \"%s\"\n", 
				__FILE__, strerror(SOCKERRNO));
		this->sockState = casOffLine;
	}
}

//
// casStreamIO::blockingState()
//
xBlockingStatus casStreamIO::blockingState() const
{
	return this->blockingFlag;
}


//
// casStreamIO::incommingBytesPresent()
//
bufSizeT casStreamIO::incommingBytesPresent() const
{
	int status;
	int nchars;

	status = socket_ioctl(this->sock, FIONREAD, &nchars);
	if (status<0) {
		char buf[64];

		/*
		 * normal conn lost conditions
		 */
		switch(SOCKERRNO){
		case ECONNABORTED:
		case ECONNRESET:
		case ETIMEDOUT:
			break;

		default:
			ipAddrToA(&this->addr.in, buf, sizeof(buf));
			ca_printf(
		"CAS: FIONREAD for %s failed because \"%s\"\n",
				buf, strerror(SOCKERRNO));
		}
		return 0u;
	}
	else if (nchars<0) {
		return 0u;
	}
	else {
		return (bufSizeT) nchars;
	}
}


//
// casStreamIO::clientHostName()
//
void casStreamIO::clientHostName (char *pInBuf, unsigned bufSizeIn) const
{
	ipAddrToA (&this->addr.in, pInBuf, bufSizeIn);
}

//
// casStreamIO:::optimumBufferSize()
//
bufSizeT casStreamIO::optimumBufferSize () 
{

#if 0
	int n;
	int size;
	int status;

        if (this->sockState!=casOnLine) {
                return MAX_TCP;
        }

	/* fetch the TCP send buffer size */
	n = sizeof(size);
	status = getsockopt(
			this->sock,
			SOL_SOCKET,
			SO_SNDBUF,
			(char *)&size,
			&n);
	if(status < 0 || n != sizeof(size)){
		size = 0x400;
	}

	if (size<=0) {
		size = 0x400;
	}
printf("the tcp buf size is %d\n", size);
	return (bufSizeT) size;
#else
// this needs to be MAX_TCP (until we fix the array problem)
	return (bufSizeT) MAX_TCP;
#endif
}

//
// casStreamIO::getFD()
//
int casStreamIO::getFD() const
{
	return this->sock;
}

//
// casStreamIO::state()
//
casIOState casStreamIO::state() const 
{
	return this->sockState;
}

