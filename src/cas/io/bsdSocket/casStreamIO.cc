//
// $Id$
//
// verify connection state prior to doing anything in this file
//
//
// $Log$
// Revision 1.18  1998/12/18 18:58:20  jhill
// fixed warning
//
// Revision 1.17  1998/11/11 01:31:59  jhill
// reduced socket buffer size
//
// Revision 1.16  1998/06/16 02:35:52  jhill
// use aToIPAddr and auto attach to winsock if its a static build
//
// Revision 1.15  1998/05/29 20:08:21  jhill
// use new sock ioctl() typedef
//
// Revision 1.14  1998/02/05 23:12:01  jhill
// use osiSock macros
//
// Revision 1.13  1997/06/30 23:40:50  jhill
// use %p for pointers
//
// Revision 1.12  1997/06/13 09:16:16  jhill
// connect proto changes
//
// Revision 1.11  1997/05/01 19:59:09  jhill
// new header file for ipAddrToA()
//
// Revision 1.10  1997/04/23 17:27:01  jhill
// use matching buffer sizes
//
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
#include "bsdSocketResource.h"

//
// casStreamIO::casStreamIO()
//
casStreamIO::casStreamIO(caServerI &cas, const ioArgsToNewStreamIO &args) :
	casStrmClient(cas), sock(args.sock), addr(args.addr), blockingFlag(xIsBlocking)
{
	assert (sock>=0);
	int yes = TRUE;
	int	status;

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
			__FILE__, SOCKERRSTR(SOCKERRNO));
		throw S_cas_internal;
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
			__FILE__, SOCKERRSTR(SOCKERRNO));
		throw S_cas_internal;
	}

	/*
	 * some concern that vxWorks will run out of mBuf's
	 * if this change is made
	 *
	 * joh 11-10-98
	 */
#if 0
	int i;

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
	    throw S_cas_internal;
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
	    throw S_cas_internal;
	}
#endif

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
outBuf::flushCondition casStreamIO::osdSend (const char *pInBuf, bufSizeT nBytesReq, 
                                 bufSizeT &nBytesActual)
{
    int	status;
    
    status = send (this->sock, (char *) pInBuf, nBytesReq, 0);
    if (status == 0) {
        return outBuf::flushDisconnect;
    }
    else if (status<0) {
        int anerrno = SOCKERRNO;
        
        if (anerrno != SOCK_EWOULDBLOCK) {
 			char buf[64];
            int errnoCpy = SOCKERRNO;

            ipAddrToA (&this->addr, buf, sizeof(buf));
			ca_printf(
	"CAS: TCP socket send to \"%s\" failed because \"%s\"\n",
				buf, SOCKERRSTR(errnoCpy));
        }
        return outBuf::flushNone;
    }
    nBytesActual = (bufSizeT) status;
    return outBuf::flushProgress;
}

//
// casStreamIO::osdRecv()
//
inBuf::fillCondition casStreamIO::osdRecv (char *pInBuf, bufSizeT nBytes, 
		bufSizeT &nBytesActual)
{
  	int nchars;

	nchars = recv (this->sock, pInBuf, nBytes, 0);
	if (nchars==0) {
		return casFillDisconnect;
	}
	else if (nchars<0) {
        int myerrno = SOCKERRNO;
		char buf[64];

        if (myerrno==SOCK_EWOULDBLOCK) {
            return casFillNone;
        }
        else  {
			ipAddrToA (&this->addr, buf, sizeof(buf));
			ca_printf(
		"CAS: client %s disconnected because \"%s\"\n",
				buf, SOCKERRSTR(myerrno));
		    return casFillDisconnect;
        }
	}
    else {
    	nBytesActual = (bufSizeT) nchars;
	    return casFillProgress;
    }
}

//
// casStreamIO::show()
//
void casStreamIO::osdShow (unsigned level) const
{
	printf ("casStreamIO at %p\n", this);
	if (level>1u) {
		char buf[64];
		ipAddrToA(&this->addr, buf, sizeof(buf));
		printf (
			"client=%s, port=%x\n",
			buf, ntohs(this->addr.sin_port));
	}
}

//
// casStreamIO::xSsetNonBlocking()
//
void casStreamIO::xSetNonBlocking()
{
	int status;
	osiSockIoctl_t yes = TRUE;

	status = socket_ioctl(this->sock, FIONBIO, &yes);
	if (status>=0) {
		this->blockingFlag = xIsntBlocking;
	}
	else {
		ca_printf("%s:CAS: TCP non blocking IO set fail because \"%s\"\n", 
				__FILE__, SOCKERRSTR(SOCKERRNO));
	    throw S_cas_internal;
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
    osiSockIoctl_t nchars;
    
    status = socket_ioctl(this->sock, FIONREAD, &nchars);
    if (status<0) {
        char buf[64];
        int errnoCpy = SOCKERRNO;
        
        ipAddrToA (&this->addr, buf, sizeof(buf) );
        ca_printf ("CAS: FIONREAD for %s failed because \"%s\"\n",
            buf, SOCKERRSTR(errnoCpy));
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
	ipAddrToA (&this->addr, pInBuf, bufSizeIn);
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
