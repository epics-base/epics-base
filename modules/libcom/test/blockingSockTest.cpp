/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>
#include <stdio.h>

#include "osiSock.h"
#include "osiWireFormat.h"
#include "epicsThread.h"
#include "epicsSignal.h"
#include "epicsUnitTest.h"
#include "testMain.h"

union address {
    struct sockaddr_in ia;
    struct sockaddr sa;
};

class circuit {
public:
    circuit ( SOCKET );
    void recvTest ();
    void shutdown ();
    void close ();
    bool recvWakeupDetected () const;
    bool sendWakeupDetected () const;
    virtual const char * pName () = 0;
protected:
    SOCKET sock;
    epicsThreadId id;
    bool recvWakeup;
    bool sendWakeup;
protected:
    virtual ~circuit() {}
};

class serverCircuit : public circuit {
public:
    serverCircuit ( SOCKET );
private:
    const char * pName ();
};

class clientCircuit : public circuit {
public:
    clientCircuit ( const address & );
private:
    const char * pName ();
};

class server {
public:
    server ( const address & );
    void start ();
    void daemon ();
    void stop ();
    address addr () const;
protected:
    address srvaddr;
    SOCKET sock;
    epicsThreadId id;
    bool exit;
};

circuit::circuit ( SOCKET sockIn ) :
    sock ( sockIn ), 
    id ( 0 ),
    recvWakeup ( false ), 
    sendWakeup ( false )
{
    testOk ( this->sock != INVALID_SOCKET, "Socket valid" );
}

bool circuit::recvWakeupDetected () const
{
    return this->recvWakeup;
}

bool circuit::sendWakeupDetected () const
{
    return this->sendWakeup;
}

void circuit::shutdown ()
{
    int status = ::shutdown ( this->sock, SHUT_RDWR );
    testOk ( status == 0, "Shutdown() returned Ok" );
}

void circuit::close ()
{
    epicsSocketDestroy ( this->sock );
}

void circuit::recvTest ()
{
    char buf [1];
    while ( true ) {
        int status = recv ( this->sock, 
            buf, (int) sizeof ( buf ), 0 );
        if ( status == 0 ) {
            testDiag ( "%s was disconnected", this->pName () );
            this->recvWakeup = true;
            break;
        }
        else if ( status > 0 ) {
            testDiag ( "%s received %i characters", this->pName (), status );
        }
        else {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            testDiag ( "%s socket recv() error was \"%s\"",
                this->pName (), sockErrBuf );
            this->recvWakeup = true;
            break;
        }
    }
}

extern "C" void socketRecvTest ( void * pParm )
{
    circuit * pCir = reinterpret_cast < circuit * > ( pParm );
    pCir->recvTest ();
}

clientCircuit::clientCircuit ( const address & addrIn ) :
    circuit ( epicsSocketCreate ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) )
{
    address tmpAddr = addrIn;
    int status = ::connect ( 
        this->sock, & tmpAddr.sa, sizeof ( tmpAddr ) );
    testOk ( status == 0, "Client end connected" );

    circuit * pCir = this;
    this->id = epicsThreadCreate ( 
        "client circuit", epicsThreadPriorityMedium, 
        epicsThreadGetStackSize(epicsThreadStackMedium), 
        socketRecvTest, pCir );
    testOk ( this->id != 0, "Client thread created" );
}


const char * clientCircuit::pName ()
{
    return "client circuit";
}

extern "C" void serverDaemon ( void * pParam ) {
    server * pSrv = reinterpret_cast < server * > ( pParam );
    pSrv->daemon ();
}

server::server ( const address & addrIn ) :
    srvaddr ( addrIn ),
    sock ( epicsSocketCreate ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ),
    id ( 0 ), exit ( false )
{
    testOk ( this->sock != INVALID_SOCKET, "Server socket valid" );

    // setup server side
    osiSocklen_t slen = sizeof ( this->srvaddr );
    int status = bind ( this->sock, & this->srvaddr.sa, slen );
    if ( status ) {
        testDiag ( "bind to server socket failed, status = %d", status );
    }
    if ( getsockname(this->sock, & this->srvaddr.sa, & slen) != 0 ) {
        testAbort ( "Failed to read socket address" );
    }
    status = listen ( this->sock, 10 );
    testOk ( status == 0, "Server socket listening" );
}

void server::start ()
{
    this->id = epicsThreadCreate ( 
        "server daemon", epicsThreadPriorityMedium, 
        epicsThreadGetStackSize(epicsThreadStackMedium), 
        serverDaemon, this );
    testOk ( this->id != 0, "Server thread created" );
}

void server::daemon ()
{
    while ( ! this->exit ) {
        // accept client side
        address addr;
        osiSocklen_t addressSize = sizeof ( addr );
        SOCKET ns = accept ( this->sock, 
            & addr.sa, & addressSize );
        if ( this->exit )
            break;
        testOk ( ns != INVALID_SOCKET, "Accepted socket valid" );
        circuit * pCir = new serverCircuit ( ns );
        testOk ( pCir != 0, "Server circuit created" );
    }
}

void server::stop ()
{
    this->exit = true;
    epicsSocketDestroy ( this->sock );
}

address server::addr () const
{
    return this->srvaddr;
}

serverCircuit::serverCircuit ( SOCKET sockIn ) :
    circuit ( sockIn )
{
    circuit * pCir = this;
    epicsThreadId threadId = epicsThreadCreate (
        "server circuit", epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        socketRecvTest, pCir );
    testOk ( threadId != 0, "Server circuit thread created" );
}

const char * serverCircuit::pName ()
{
    return "server circuit";
}

static const char *mechName(int mech)
{
    static const struct {
        int mech;
        const char *name;
    } mechs[] = {
        {-1, "Unknown shutdown mechanism" },
        {esscimqi_socketCloseRequired, "esscimqi_socketCloseRequired" },
        {esscimqi_socketBothShutdownRequired, "esscimqi_socketBothShutdownRequired" },
        {esscimqi_socketSigAlarmRequired, "esscimqi_socketSigAlarmRequired" }
    };

    for (unsigned i=0; i < (sizeof(mechs) / sizeof(mechs[0])); ++i) {
        if (mech == mechs[i].mech)
            return mechs[i].name;
    }
    return "Unknown shutdown mechanism value";
}

MAIN(blockingSockTest)
{
    testPlan(13);
    osiSockAttach();

    address addr;
    memset ( (char *) & addr, 0, sizeof ( addr ) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK ); 
    addr.ia.sin_port = 0;

    server srv ( addr );
    srv.start ();
    addr = srv.addr ();
    clientCircuit client ( addr );

    epicsThreadSleep ( 1.0 );
    testOk ( ! client.recvWakeupDetected (), "Client is asleep" );

    testDiag("Trying Shutdown mechanism");
    client.shutdown ();
    epicsThreadSleep ( 1.0 );
    int mech = -1;
    if ( client.recvWakeupDetected () ) {
        mech = esscimqi_socketBothShutdownRequired;
        testDiag("Shutdown succeeded");
    }
    else {
        testDiag("Trying Close mechanism");
        client.close ();
        epicsThreadSleep ( 1.0 );
        if ( client.recvWakeupDetected () ) {
            mech = esscimqi_socketCloseRequired;
            testDiag("Close succeeded");
        }
    }
    testDiag("This OS behaves like \"%s\".", mechName(mech));

    int query = epicsSocketSystemCallInterruptMechanismQuery ();
    if (! testOk(mech == query, "Declared mechanism works") )
        testDiag("epicsSocketSystemCallInterruptMechanismQuery returned \"%s\"",
            mechName(query));

    srv.stop ();
    epicsThreadSleep ( 1.0 );

    osiSockRelease();
    return testDone();
}

