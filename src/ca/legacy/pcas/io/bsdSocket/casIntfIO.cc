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

#include <stdio.h>

#include "errlog.h"
#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"
#include "epicsAssert.h"

#define epicsExportSharedSymbols
#include "casdef.h"
#include "caNetAddr.h"
#include "casIntfIO.h"
#include "casStreamIO.h"
#include "casStreamOS.h"

//
// 5 appears to be a TCP/IP built in maximum
//
const unsigned caServerConnectPendQueueSize = 5u;

//
// casIntfIO::casIntfIO()
//
casIntfIO::casIntfIO ( const caNetAddr & addrIn ) :
    sock ( INVALID_SOCKET ),
    addr ( addrIn.getSockIP() )
{
    int status;
    osiSocklen_t addrSize;
    bool portChange;

    if ( ! osiSockAttach () ) {
        throw S_cas_internal;
    }

    /*
     * Setup the server socket
     */
    this->sock = epicsSocketCreate ( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if (this->sock == INVALID_SOCKET) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
        printf ( "No socket error was %s\n", sockErrBuf );
        throw S_cas_noFD;
    }

    epicsSocketEnableAddressReuseDuringTimeWaitState ( this->sock );

    status = bind ( this->sock,
                    reinterpret_cast <sockaddr *> (&this->addr),
                    sizeof(this->addr) );
    if (status < 0) {
        if (SOCKERRNO == SOCK_EADDRINUSE ||
            SOCKERRNO == SOCK_EACCES) {
            //
            // enable assignment of a default port
            // (so the getsockname() call below will
            // work correctly)
            //
            this->addr.sin_port = ntohs (0);
            status = bind(
                    this->sock,
                    reinterpret_cast <sockaddr *> (&this->addr),
                    sizeof(this->addr) );
        }
        if (status < 0) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            char buf[64];
            ipAddrToA (&this->addr, buf, sizeof(buf));
            errlogPrintf ( "CAS: Socket bind TCP to %s failed with %s",
                buf, sockErrBuf );
            epicsSocketDestroy (this->sock);
            throw S_cas_bindFail;
        }
        portChange = true;
    }
    else {
        portChange = false;
    }

    addrSize = ( osiSocklen_t ) sizeof (this->addr);
    status = getsockname (
                this->sock,
                reinterpret_cast <sockaddr *> ( &this->addr ),
                &addrSize );
    if (status) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ( "CAS: getsockname() error %s\n",
            sockErrBuf );
        epicsSocketDestroy (this->sock);
        throw S_cas_internal;
    }

    //
    // be sure of this now so that we can fetch the IP
    // address and port number later
    //
    assert (this->addr.sin_family == AF_INET);

    if ( portChange ) {
        errlogPrintf ( "cas warning: Configured TCP port was unavailable.\n");
        errlogPrintf ( "cas warning: Using dynamically assigned TCP port %hu,\n",
            ntohs (this->addr.sin_port) );
        errlogPrintf ( "cas warning: but now two or more servers share the same UDP port.\n");
        errlogPrintf ( "cas warning: Depending on your IP kernel this server may not be\n" );
        errlogPrintf ( "cas warning: reachable with UDP unicast (a host's IP in EPICS_CA_ADDR_LIST)\n" );
    }

    status = listen(this->sock, caServerConnectPendQueueSize);
    if (status < 0) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ( "CAS: listen() error %s\n", sockErrBuf );
        epicsSocketDestroy (this->sock);
        throw S_cas_internal;
    }
}

//
// casIntfIO::~casIntfIO()
//
casIntfIO::~casIntfIO()
{
    if (this->sock != INVALID_SOCKET) {
        epicsSocketDestroy (this->sock);
    }

    osiSockRelease ();
}

//
// newStreamIO::newStreamClient()
//
casStreamOS *casIntfIO::newStreamClient ( caServerI & cas,
                               clientBufMemoryManager & bufMgr ) const
{
    static bool oneMsgFlag = false;

    struct sockaddr newClientAddr;
    osiSocklen_t length = ( osiSocklen_t ) sizeof ( newClientAddr );
    SOCKET newSock = epicsSocketAccept ( this->sock, & newClientAddr, & length );
    if ( newSock == INVALID_SOCKET ) {
        int errnoCpy = SOCKERRNO;
        if ( errnoCpy != SOCK_EWOULDBLOCK && ! oneMsgFlag ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ( "CAS: %s accept error \"%s\"\n",
                __FILE__, sockErrBuf );
            oneMsgFlag = true;
        }
        return NULL;
    }
    else if ( sizeof ( newClientAddr ) > (size_t) length ) {
        epicsSocketDestroy ( newSock );
        errlogPrintf ( "CAS: accept returned bad address len?\n" );
        return NULL;
    }
    oneMsgFlag = false;
    ioArgsToNewStreamIO args;
    args.clientAddr = newClientAddr;
    args.sock = newSock;
    casStreamOS	* pOS = new casStreamOS ( cas, bufMgr, args );
    if ( ! pOS ) {
        errMessage ( S_cas_noMemory,
            "unable to create data structures for a new client" );
        epicsSocketDestroy ( newSock );
    }
    else {
        if ( cas.getDebugLevel() > 0u ) {
            char pName[64u];

            pOS->hostName ( pName, sizeof ( pName ) );
            errlogPrintf ( "CAS: allocated client object for \"%s\"\n", pName );
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
    osiSockIoctl_t yes = true;

    status = socket_ioctl(this->sock, FIONBIO, &yes);
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf (
            "%s:CAS: server non blocking IO set fail because \"%s\"\n",
            __FILE__, sockErrBuf );
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
// casIntfIO::serverAddress ()
// (avoid problems with GNU inliner)
//
caNetAddr casIntfIO::serverAddress () const
{
    return caNetAddr (this->addr);
}
