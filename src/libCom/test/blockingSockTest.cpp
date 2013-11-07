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
    struct sockaddr_in m_ia;
    struct sockaddr m_sa;
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
    SOCKET m_sock;
    epicsThreadId m_id;
    bool m_recvWakeup;
    bool m_sendWakeup;
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
    address addr () const;
protected:
    address m_addr;
    SOCKET m_sock;
    epicsThreadId m_id;
    bool m_exit;
};

circuit::circuit ( SOCKET sockIn ) :
    m_sock ( sockIn ), 
    m_id ( 0 ),
    m_recvWakeup ( false ), 
    m_sendWakeup ( false )
{
    verify ( m_sock != INVALID_SOCKET );
}

bool circuit::recvWakeupDetected () const
{
    return m_recvWakeup;
}

bool circuit::sendWakeupDetected () const
{
    return m_sendWakeup;
}

void circuit::shutdown ()
{
    int status = ::shutdown ( m_sock, SHUT_RDWR );
    verify ( status == 0 );
}

void circuit::signal ()
{
    epicsSignalRaiseSigAlarm ( m_id );
}

void circuit::close ()
{
    epicsSocketDestroy ( m_sock );
}

void circuit::recvTest ()
{
    epicsSignalInstallSigAlarmIgnore ();
    char buf [1];
    while ( true ) {
        int status = recv ( m_sock, 
            buf, (int) sizeof ( buf ), 0 );
        if ( status == 0 ) {
            testDiag ( "%s was disconnected", this->pName () );
            m_recvWakeup = true;
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
            m_recvWakeup = true;
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
        m_sock, & tmpAddr.m_sa, sizeof ( tmpAddr ) );
    verify ( status == 0 );

    circuit * pCir = this;
    m_id = epicsThreadCreate ( 
        "client circuit", epicsThreadPriorityMedium, 
        epicsThreadGetStackSize(epicsThreadStackMedium), 
        socketRecvTest, pCir );
    verify ( m_id );
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
    m_addr ( addrIn ),
    m_sock ( epicsSocketCreate ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ),
    m_id ( 0 ), m_exit ( false )
{
    verify ( m_sock != INVALID_SOCKET );

    // setup server side
    int status = bind ( m_sock,
        & m_addr.m_sa, sizeof ( m_addr ) );
    if ( status ) {
        testDiag ( "bind to server socket failed, status = %d", status );
    }
    osiSocklen_t slen = sizeof ( m_addr );
    if ( getsockname(m_sock, &m_addr.m_sa, &slen) != 0 ) {
        testAbort ( "Failed to read socket address" );
    }
    status = listen ( m_sock, 10 );
    verify ( status == 0 );
}

void server::start ()
{
    m_id = epicsThreadCreate (
        "server daemon", epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        serverDaemon, this );
    verify ( m_id );
}

void server::daemon ()
{
    while ( ! m_exit ) {
        // accept client side
        address addr;
        osiSocklen_t addressSize = sizeof ( addr );
        SOCKET ns = accept ( m_sock,
            & m_addr.m_sa, & addressSize );
        verify ( ns != INVALID_SOCKET );
        circuit * pCir = new serverCircuit ( ns );
        verify ( pCir );
    }
}

address server::addr () const
{
    return m_addr;
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
    addr.m_ia.sin_family = AF_INET;
    addr.m_ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK ); 
    addr.m_ia.sin_port = 0;

    server srv ( addr );
    srv.start ();
    addr = srv.addr ();
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

