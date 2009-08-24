/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "osiSock.h"
#include "osiWireFormat.h"
#include "epicsThread.h"
#include "epicsSignal.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define verify(exp) ((exp) ? (void)0 : \
    epicsAssert(__FILE__, __LINE__, #exp, epicsAssertAuthor))

union address {
    struct sockaddr_in ia;
    struct sockaddr sa;
};

class circuit {
public:
    circuit ( SOCKET );
    void recvTest ();
    void shutdown ();
    void signal ();
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
protected:
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
    verify ( this->sock != INVALID_SOCKET );
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
    verify ( status == 0 );
}

void circuit::signal ()
{
    epicsSignalRaiseSigAlarm ( this->id );
}

void circuit::close ()
{
    epicsSocketDestroy ( this->sock );
}

void circuit::recvTest ()
{
    epicsSignalInstallSigAlarmIgnore ();
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
            testDiag ( "client received %i characters", status );
        }
        else {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            testDiag ( "%s socket recv() error was \"%s\"\n",
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
    verify ( status == 0 );

    circuit * pCir = this;
    this->id = epicsThreadCreate ( 
        "client circuit", epicsThreadPriorityMedium, 
        epicsThreadGetStackSize(epicsThreadStackMedium), 
        socketRecvTest, pCir );
    verify ( this->id );
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
    sock ( epicsSocketCreate ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ),
    id ( 0 ), exit ( false )
{
    verify ( this->sock != INVALID_SOCKET );

    // setup server side
    address tmpAddr = addrIn;
    int status = bind ( this->sock, 
        & tmpAddr.sa, sizeof ( tmpAddr ) );
    if ( status ) {
        testDiag ( "bind to server socket failed, status = %d", status );
        testAbort ( "Stop all CA servers before running this test." );
    }
    status = listen ( this->sock, 10 );
    verify ( status == 0 );
}

void server::start ()
{
    this->id = epicsThreadCreate ( 
        "server daemon", epicsThreadPriorityMedium, 
        epicsThreadGetStackSize(epicsThreadStackMedium), 
        serverDaemon, this );
    verify ( this->id );
}

void server::daemon () 
{
    while ( ! this->exit ) {
        // accept client side
        address addr;
        osiSocklen_t addressSize = sizeof ( addr );
        SOCKET ns = accept ( this->sock, 
            & addr.sa, & addressSize );
        verify ( ns != INVALID_SOCKET );
        circuit * pCir = new serverCircuit ( ns );
        verify ( pCir );
    }
}

serverCircuit::serverCircuit ( SOCKET sockIn ) :
    circuit ( sockIn )
{
    circuit * pCir = this;
    epicsThreadId threadId = epicsThreadCreate ( 
        "server circuit", epicsThreadPriorityMedium, 
        epicsThreadGetStackSize(epicsThreadStackMedium), 
        socketRecvTest, pCir );
    verify ( threadId );
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
    testPlan(1);

    address addr;
    memset ( (char *) & addr, 0, sizeof ( addr ) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK ); 
    addr.ia.sin_port = htons ( 5064 ); // CA

    server srv ( addr );
    srv.start ();
    clientCircuit client ( addr );

    epicsThreadSleep ( 1.0 );
    verify ( ! client.recvWakeupDetected () );

    client.shutdown ();
    epicsThreadSleep ( 1.0 );
    int mech = -1;
    if ( client.recvWakeupDetected () ) {
        mech = esscimqi_socketBothShutdownRequired;
    }
    else {
        client.signal ();
        epicsThreadSleep ( 1.0 );
        if ( client.recvWakeupDetected () ) {
            mech = esscimqi_socketSigAlarmRequired;
        }
        else {
            client.close ();
            epicsThreadSleep ( 1.0 );
            if ( client.recvWakeupDetected () ) {
                mech = esscimqi_socketCloseRequired;
            }
        }
    }
    testDiag("This OS behaves like \"%s\".", mechName(mech));

    int query = epicsSocketSystemCallInterruptMechanismQuery ();
    if (! testOk(mech == query, "Socket shutdown mechanism") )
        testDiag("epicsSocketSystemCallInterruptMechanismQuery returned \"%s\"",
            mechName(query));

    return testDone();
}

