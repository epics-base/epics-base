
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "osiSock.h"
#include "osiWireFormat.h"
#include "epicsThread.h"
#include "epicsSignal.h"

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
    assert ( this->sock != INVALID_SOCKET );
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
    assert ( status == 0 );
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
            printf ( "%s: %s was disconnected\n",
                __FILE__, this->pName () );
            this->recvWakeup = true;
            break;
        }
        else if ( status > 0 ) {
            printf ( "%s: client received %i characters\n", 
                __FILE__, status );
        }
        else {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            printf ( "%s: %s socket recv() error was \"%s\"\n",
                __FILE__, this->pName (), sockErrBuf );
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
    assert ( status == 0 );

    circuit * pCir = this;
    this->id = epicsThreadCreate ( 
        "client circuit", epicsThreadPriorityMedium, 
        epicsThreadGetStackSize(epicsThreadStackMedium), 
        socketRecvTest, pCir );
    assert ( this->id );
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
    assert ( this->sock != INVALID_SOCKET );

    // setup server side
    address tmpAddr = addrIn;
    int status = bind ( this->sock, 
        & tmpAddr.sa, sizeof ( tmpAddr ) );
    assert ( status == 0 );
    status = listen ( this->sock, 10 );
    assert ( status == 0 );

    this->id = epicsThreadCreate ( 
        "server daemon", epicsThreadPriorityMedium, 
        epicsThreadGetStackSize(epicsThreadStackMedium), 
        serverDaemon, this );
    assert ( this->id );
}

void server::daemon () 
{
    while ( ! this->exit ) {
        // accept client side
        address addr;
        osiSocklen_t addressSize = sizeof ( addr );
        SOCKET ns = accept ( this->sock, 
            & addr.sa, & addressSize );
        assert ( ns != INVALID_SOCKET );
        new serverCircuit ( ns );
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
    assert ( threadId );
}

const char * serverCircuit::pName ()
{
    return "server circuit";
}

void blockingSockTest ()
{
    address addr;
    memset ( (char *) & addr, 0, sizeof ( addr ) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = epicsHTON32 ( INADDR_LOOPBACK ); 
    //addr.ia.sin_addr.s_addr = epicsHTON32 ( 0x80A5A07F ); 
    addr.ia.sin_port = epicsHTON16 ( 5064 ); // CA

    server srv ( addr );
    clientCircuit client ( addr );

    epicsThreadSleep ( 1.0 );
    assert ( ! client.recvWakeupDetected () );

    client.shutdown ();
    epicsThreadSleep ( 1.0 );
    const char * pStr = "esscimqi_?????";
    if ( client.recvWakeupDetected () ) {
        pStr = "esscimqi_socketBothShutdownRequired";
    }
    else {
        client.signal ();
        epicsThreadSleep ( 1.0 );
        if ( client.recvWakeupDetected () ) {
            pStr = "esscimqi_socketSigAlarmRequired";
        }
        else {
            client.close ();
            epicsThreadSleep ( 1.0 );
            if ( client.recvWakeupDetected () ) {
                pStr = "esscimqi_socketCloseRequired";
            }
            else {
                pStr = "esscimqi_?????";
            }
        }
    }

    printf ( "The local OS behaves like \"%s\".\n", pStr );
    pStr = "esscimqi_?????";
    switch ( epicsSocketSystemCallInterruptMechanismQuery() ) {
    case esscimqi_socketCloseRequired:
        pStr = "esscimqi_socketCloseRequired";
        break;
    case esscimqi_socketBothShutdownRequired:
        pStr = "esscimqi_socketBothShutdownRequired";
        break;
    case esscimqi_socketSigAlarmRequired:
        pStr = "esscimqi_socketSigAlarmRequired";
        break;
    }
    printf ( "The epicsSocketSystemCallInterruptMechanismQuery() function returns\n\"%s\".\n",
        pStr );
}
