/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
// Author: Jeff Hill
//

#include "errlog.h"

#define epicsExportSharedSymbols
#include "casStreamIO.h"

// casStreamIO::casStreamIO()
casStreamIO::casStreamIO ( caServerI & cas, clientBufMemoryManager & bufMgr,
                           const ioArgsToNewStreamIO & args ) :
    casStrmClient ( cas, bufMgr, args.clientAddr ),
    sock ( args.sock ),  
    _osSendBufferSize ( MAX_TCP ), 
    blockingFlag ( xIsBlocking ),
    sockHasBeenShutdown ( false )
{
	assert ( sock >= 0 );
	int yes = true;
	int	status;

	/*
	 * see TCP(4P) this seems to make unsollicited single events much
	 * faster. I take care of queue up as load increases.
	 */
	status = setsockopt ( this->sock, IPPROTO_TCP, TCP_NODELAY,
							( char * ) & yes, sizeof ( yes ) );
	if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
		errlogPrintf (
			"CAS: %s TCP_NODELAY option set failed %s\n",
			__FILE__, sockErrBuf );
		throw S_cas_internal;
	}

	/*
	 * turn on KEEPALIVE so if the client crashes
	 * this task will find out and exit
	 */
	status = setsockopt ( sock, SOL_SOCKET, SO_KEEPALIVE,
					(char *) & yes, sizeof ( yes ) );
	if (status<0) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
		errlogPrintf (
			"CAS: %s SO_KEEPALIVE option set failed %s\n",
			__FILE__, sockErrBuf );
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
			errlogPrintf("CAS: SO_SNDBUF set failed\n");
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
		errlogPrintf("CAS: SO_RCVBUF set failed\n");
	    throw S_cas_internal;
	}
#endif
    
	/* cache the TCP send buffer size */
	int size = MAX_TCP;
	osiSocklen_t n = sizeof ( size ) ;
	status = getsockopt ( this->sock, SOL_SOCKET,
			SO_SNDBUF, reinterpret_cast < char * > ( & size ), & n );
	if ( status < 0 || n != sizeof ( size ) ) {
		size = MAX_TCP;
	}
	if ( size <= MAX_TCP ) {
		size = MAX_TCP;
	}
	_osSendBufferSize = static_cast < bufSizeT > ( size );
}

// casStreamIO::~casStreamIO()
casStreamIO::~casStreamIO()
{
	epicsSocketDestroy ( this->sock );
}

// casStreamIO::osdSend()
outBufClient::flushCondition casStreamIO::osdSend ( const char *pInBuf, bufSizeT nBytesReq, 
                                 bufSizeT &nBytesActual )
{
    int	status;
    
    if ( nBytesReq == 0 ) {
        nBytesActual = 0;
        return outBufClient::flushNone;
    }
    
    status = send (this->sock, (char *) pInBuf, nBytesReq, 0);
    if (status == 0) {
        return outBufClient::flushDisconnect;
    }
    else if (status<0) {
        int anerrno = SOCKERRNO;
        
        if ( anerrno == SOCK_EINTR || 
                anerrno == SOCK_EWOULDBLOCK ) {
            return outBufClient::flushNone;
        }

        if ( anerrno == SOCK_ENOBUFS ) {
            errlogPrintf ( 
                "cas: system low on network buffers - hybernating for 1 second\n" );
            epicsThreadSleep ( 1.0 );
            return outBufClient::flushNone;
        }

        if (  
            anerrno != SOCK_ECONNABORTED &&
            anerrno != SOCK_ECONNRESET &&
            anerrno != SOCK_EPIPE &&
            anerrno != SOCK_ETIMEDOUT ) {

            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
 			char buf[64];
            this->hostName ( buf, sizeof ( buf ) );
			errlogPrintf (
"CAS: TCP socket send to \"%s\" failed because \"%s\"\n",
				buf, sockErrBuf );
        }
        return outBufClient::flushDisconnect;
    }
    nBytesActual = (bufSizeT) status;
    return outBufClient::flushProgress;
}

// casStreamIO::osdRecv()
inBufClient::fillCondition
casStreamIO::osdRecv ( char * pInBuf, bufSizeT nBytes, // X aCC 361
                      bufSizeT & nBytesActual )
{
    int nchars;
    
    nchars = recv (this->sock, pInBuf, nBytes, 0);
    if ( nchars == 0 ) {
        return casFillDisconnect;
    }
    else if ( nchars < 0 ) {
        int myerrno = SOCKERRNO;
        char buf[64];

        if ( myerrno == SOCK_EWOULDBLOCK ||
                myerrno == SOCK_EINTR ) {
            return casFillNone;
        }

        if ( myerrno == SOCK_ENOBUFS ) {
            errlogPrintf ( 
                "CAS: system low on network buffers - hybernating for 1 second\n" );
            epicsThreadSleep ( 1.0 );
            return casFillNone;
        }

        if (
            myerrno != SOCK_ECONNABORTED &&
            myerrno != SOCK_ECONNRESET &&
            myerrno != SOCK_EPIPE &&
            myerrno != SOCK_ETIMEDOUT ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            this->hostName ( buf, sizeof ( buf ) );
            errlogPrintf(
		"CAS: client %s disconnected because \"%s\"\n",
                buf, sockErrBuf );
        }
        return casFillDisconnect;
    }
    else {
    	nBytesActual = (bufSizeT) nchars;
        return casFillProgress;
    }
}

// casStreamIO::forceDisconnect()
void casStreamIO::forceDisconnect ()
{
    // !!!! other OS specific wakeup will be required here when we 
    // !!!! switch to a threaded implementation
	if ( ! this->sockHasBeenShutdown ) {
        int status = ::shutdown ( this->sock, SHUT_RDWR );
        if ( status == 0 ) {
            this->sockHasBeenShutdown = true;
        }
        else {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ("CAC TCP socket shutdown error was %s\n", 
                sockErrBuf );
        }
	}
}

// casStreamIO::show()
void casStreamIO::osdShow (unsigned level) const
{
	printf ( "casStreamIO at %p\n", 
        static_cast <const void *> ( this ) );
	if (level>1u) {
		char buf[64];
		this->hostName ( buf, sizeof ( buf ) );
		printf ( "client = \"%s\"\n", buf );
	}
}

// casStreamIO::xSsetNonBlocking()
void casStreamIO::xSetNonBlocking()
{
	int status;
	osiSockIoctl_t yes = true;

	status = socket_ioctl(this->sock, FIONBIO, &yes); // X aCC 392
	if (status>=0) {
		this->blockingFlag = xIsntBlocking;
	}
	else {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
		errlogPrintf ( "%s:CAS: TCP non blocking IO set fail because \"%s\"\n", 
				__FILE__, sockErrBuf );
	    throw S_cas_internal;
	}
}

// casStreamIO::blockingState()
xBlockingStatus casStreamIO::blockingState() const
{
	return this->blockingFlag;
}
	
// casStreamIO :: inCircuitBytesPending()
bufSizeT casStreamIO :: inCircuitBytesPending () const 
{
    int status;
    osiSockIoctl_t nchars = 0;
    
    status = socket_ioctl ( this->sock, FIONREAD, &nchars ); 
    if ( status < 0 ) {
        int localError = SOCKERRNO;
        if (
            localError != SOCK_ECONNABORTED &&
            localError != SOCK_ECONNRESET &&
            localError != SOCK_EPIPE &&
            localError != SOCK_ETIMEDOUT ) 
        {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            char buf[64];
            this->hostName ( buf, sizeof ( buf ) );
            errlogPrintf ("CAS: FIONREAD for %s failed because \"%s\"\n",
                buf, sockErrBuf );
        }
        return 0u;
    }
    else if ( nchars < 0 ) {
        return 0u;
    }
    else {
        return ( bufSizeT ) nchars;
    }
}

// casStreamIO :: osSendBufferSize ()
bufSizeT casStreamIO :: osSendBufferSize () const 
{
    return _osSendBufferSize;
}

// casStreamIO::getFD()
int casStreamIO::getFD() const
{
	return this->sock;
}
