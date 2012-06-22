/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *
 * REPEATER.cpp
 *
 * CA broadcast repeater
 *
 * Author:  Jeff Hill
 * Date:    3-27-90
 *
 *  Control System Software for the GTA Project
 *
 *  Copyright 1988, 1989, the Regents of the University of California.
 *
 *  This software was produced under a U.S. Government contract
 *  (W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *  operated by the University of California for the U.S. Department
 *  of Energy.
 *
 *  Developed by the Controls and Automation Group (AT-8)
 *  Accelerator Technology Division
 *  Los Alamos National Laboratory
 *
 *  Direct inqueries to:
 *  Jeff HIll, AT-8, Mail Stop H820
 *  Los Alamos National Laboratory
 *  Los Alamos, New Mexico 87545
 *  Phone: (505) 665-1831
 *  E-mail: johill@lanl.gov
 *
 *  PURPOSE:
 *  Broadcasts fan out over the LAN, but old IP kernels do not allow
 *  two processes on the same machine to get the same broadcast
 *  (and modern IP kernels do not allow two processes on the same machine
 *  to receive the same unicast).
 *
 *  This code fans out UDP messages sent to the CA repeater port
 *  to all CA client processes that have subscribed.
 *
 */

/*
 * It would be preferable to avoid using the repeater on multicast enhanced
 * IP kernels, but this is not going to work in all situations because
 * (according to Steven's TCP/IP illustrated volume I) if a broadcast is
 * received it goes to all sockets on the same port, but if a unicast is
 * received it goes to only one of the sockets on the same port (we can only
 * guess at which one it will be).
 *   
 * I have observed this behavior under winsock II:
 * o only one of the sockets on the same port receives the message if we
 *   send to the loopback address
 * o both of the sockets on the same port receives the message if we send
 *   to the broadcast address
 */

/* verifyClients() Mechanism
 *
 * This is required because Solaris and HPUX have half baked versions
 * of sockets.
 *
 * As written, the repeater should be robust against situations where the
 * IP kernel doesn't implement UDP disconnect on receiving ICMP port
 * unreachable errors from the destination process. As I recall, this
 * change was required in the repeater code when we ported from sunos4 to
 * Solaris. To avoid unreasonable overhead, I decided at the time to check
 * the validity of all existing connections only when a new client
 * registers with the repeater (and not when fanning out each beacon
 * received).                                           -- Jeff
 */

#include <string>
#include <stdexcept>
#include <stdio.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "tsDLList.h"
#include "envDefs.h"
#include "tsFreeList.h"
#include "osiWireFormat.h"
#include "taskwd.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "caProto.h"
#include "udpiiu.h"
#include "repeaterClient.h"


/* 
 *  these can be external since there is only one instance
 *  per machine so we dont care about reentrancy
 */
static tsDLList < repeaterClient > client_list;

static const unsigned short PORT_ANY = 0u;

/*
 * makeSocket()
 */
static bool makeSocket ( unsigned short port, bool reuseAddr, SOCKET * pSock )
{
    int status;
	union {
		struct sockaddr_in ia;
		struct sockaddr sa;
	} bd;

    SOCKET sock = epicsSocketCreate ( AF_INET, SOCK_DGRAM, 0 );     
    if ( sock == INVALID_SOCKET ) {
        return false;
    }

    /*
     * no need to bind if unconstrained
     */
    if ( port != PORT_ANY ) {

        memset ( (char *) &bd, 0, sizeof (bd) );
        bd.ia.sin_family = AF_INET;
        bd.ia.sin_addr.s_addr = htonl ( INADDR_ANY ); 
        bd.ia.sin_port = htons ( port );  
        status = bind ( sock, &bd.sa, (int) sizeof(bd) );
        if ( status < 0 ) {
            epicsSocketDestroy ( sock );
            return false;
        }
        if ( reuseAddr ) {
            epicsSocketEnableAddressReuseDuringTimeWaitState ( sock );
        }
    }
    *pSock = sock;
    return true;
}

repeaterClient::repeaterClient ( const osiSockAddr &fromIn ) :
    from ( fromIn ), sock ( INVALID_SOCKET )
{
#ifdef DEBUG
    unsigned port = ntohs ( from.ia.sin_port );
    debugPrintf ( ( "new client %u\n", port ) );
#endif
}

bool repeaterClient::connect ()
{
    int status;

    if ( ! makeSocket ( PORT_ANY, false, & this->sock ) ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf ( stderr, "%s: no client sock because \"%s\"\n",
                __FILE__, sockErrBuf );
        return false;
    }

    status = ::connect ( this->sock, &this->from.sa, sizeof ( this->from.sa ) );
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf ( stderr, "%s: unable to connect client sock because \"%s\"\n",
            __FILE__, sockErrBuf );
        return false;
    }

    return true;
}

bool repeaterClient::sendConfirm ()
{
    int status;

    caHdr confirm;
    memset ( (char *) &confirm, '\0', sizeof (confirm) );
    AlignedWireRef < epicsUInt16 > ( confirm.m_cmmd ) = REPEATER_CONFIRM;
    confirm.m_available = this->from.ia.sin_addr.s_addr;
    status = send ( this->sock, (char *) &confirm,
                    sizeof (confirm), 0 );
    if ( status >= 0 ) {
        assert ( status == sizeof ( confirm ) );
        return true;
    }
    else if ( SOCKERRNO == SOCK_ECONNREFUSED ) {
        return false;
    }
    else {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        debugPrintf ( ( "CA Repeater: confirm req err was \"%s\"\n", sockErrBuf) );
        return false;
    }
}

bool repeaterClient::sendMessage ( const void *pBuf, unsigned bufSize )
{
    int status;

    status = send ( this->sock, (char *) pBuf, bufSize, 0 );
    if ( status >= 0 ) {
        assert ( static_cast <unsigned> ( status ) == bufSize );
#ifdef DEBUG
        epicsUInt16 port = ntohs ( this->from.ia.sin_port );
        debugPrintf ( ("Sent to %u\n", port ) );
#endif
        return true;
    }
    else {
        int errnoCpy = SOCKERRNO;
        if ( errnoCpy == SOCK_ECONNREFUSED ) {
#ifdef DEBUG
            epicsUInt16 port = ntohs ( this->from.ia.sin_port );
            debugPrintf ( ("Client refused message %u\n", port ) );
#endif
        }
        else {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            debugPrintf ( ( "CA Repeater: UDP send err was \"%s\"\n", sockErrBuf) );
        }
        return false;
    }
}

repeaterClient::~repeaterClient ()
{
    if ( this->sock != INVALID_SOCKET ) {
        epicsSocketDestroy ( this->sock );
    }
#ifdef DEBUG
    epicsUInt16 port = ntohs ( this->from.ia.sin_port );
    debugPrintf ( ( "Deleted client %u\n", port ) );
#endif
}

void repeaterClient::operator delete ( void * )
{ 
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

void * repeaterClient::operator new ( size_t size, 
    tsFreeList < repeaterClient, 0x20 > & freeList )
{ 
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
void repeaterClient::operator delete ( void *pCadaver, 
    tsFreeList < repeaterClient, 0x20 > & freeList ) 
{ 
    freeList.release ( pCadaver );
}
#endif

inline unsigned short repeaterClient::port () const
{
    return ntohs ( this->from.ia.sin_port );
}

inline bool repeaterClient::identicalAddress ( const osiSockAddr &fromIn )
{
    if ( fromIn.sa.sa_family == this->from.sa.sa_family ) {
        if ( fromIn.ia.sin_port == this->from.ia.sin_port) {
            if ( fromIn.ia.sin_addr.s_addr == this->from.ia.sin_addr.s_addr ) {
                return true;
            }
        }
    }
    return false;
}

inline bool repeaterClient::identicalPort ( const osiSockAddr &fromIn )
{
    if ( fromIn.sa.sa_family == this->from.sa.sa_family ) {
        if ( fromIn.ia.sin_port == this->from.ia.sin_port) {
            return true;
        }
    }
    return false;
}

bool repeaterClient::verify ()
{
    SOCKET tmpSock;
    bool success = makeSocket ( this->port (), false, & tmpSock );
    if ( success ) {
        epicsSocketDestroy ( tmpSock );
    }
    else {
        if ( SOCKERRNO != SOCK_EADDRINUSE ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            fprintf ( stderr, "CA Repeater: bind test err was \"%s\"\n", 
                sockErrBuf );
        }
    }
    return ! success;
}


/*
 * verifyClients()
 */
static void verifyClients ( tsFreeList < repeaterClient, 0x20 > & freeList )
{
    static tsDLList < repeaterClient > theClients;
    repeaterClient *pclient;

    while ( ( pclient = client_list.get () ) ) {
        if ( pclient->verify () ) {
            theClients.add ( *pclient );
        }
        else {
            pclient->~repeaterClient ();
            freeList.release ( pclient );
        }
    }
    client_list.add ( theClients );
}

/*
 * fanOut()
 */
static void fanOut ( const osiSockAddr & from, const void * pMsg, 
    unsigned msgSize, tsFreeList < repeaterClient, 0x20 > & freeList )
{
    static tsDLList < repeaterClient > theClients;
    repeaterClient *pclient;

    while ( ( pclient = client_list.get () ) ) {
        theClients.add ( *pclient );
        /* Dont reflect back to sender */
        if ( pclient->identicalAddress ( from ) ) {
            continue;
        }

        if ( ! pclient->sendMessage ( pMsg, msgSize ) ) {
            if ( ! pclient->verify () ) {
                theClients.remove ( *pclient );
                pclient->~repeaterClient ();
                freeList.release ( pclient );
            }
        }
    }

    client_list.add ( theClients );
}

/*
 * register_new_client()
 */
static void register_new_client ( osiSockAddr & from, 
            tsFreeList < repeaterClient, 0x20 > & freeList )
{
    bool newClient = false;
    int status;

    if ( from.sa.sa_family != AF_INET ) {
        return;
    }

    /*
     * the repeater and its clients must be on the same host
     */
    if ( INADDR_LOOPBACK != ntohl ( from.ia.sin_addr.s_addr ) ) {
        static SOCKET testSock = INVALID_SOCKET;
        static bool init = false;

        if ( ! init ) {
            SOCKET sock;
            if ( ! makeSocket ( PORT_ANY, true, & sock ) ) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( 
                    sockErrBuf, sizeof ( sockErrBuf ) );
                fprintf ( stderr, "%s: Unable to create repeater bind test socket because \"%s\"\n",
                    __FILE__, sockErrBuf );
            }
            else {
                testSock = sock;
            }
            init = true;
        }

        /*
         * Unfortunately on 3.13 beta 11 and before the
         * repeater would not always allow the loopback address
         * as a local client address so current clients alternate
         * between the address of the first non-loopback interface
         * found and the loopback addresss when subscribing with 
         * the CA repeater until all CA repeaters have been updated
         * to current code.
         */
        if ( testSock != INVALID_SOCKET ) {
            osiSockAddr addr;

            addr = from;
            addr.ia.sin_port = PORT_ANY;

            /* we can only bind to a local address */
            status = bind ( testSock, &addr.sa, sizeof ( addr ) );
            if ( status ) {
                return;
            }
        }
        else {
            return;
        }
    }

    tsDLIter < repeaterClient > pclient = client_list.firstIter ();
    while ( pclient.valid () ) {
        if ( pclient->identicalPort ( from ) ) {
            break;
        }
        pclient++;
    }      
    
    repeaterClient *pNewClient;
    if ( pclient.valid () ) {
        pNewClient = pclient.pointer ();
    }
    else {
        pNewClient = new ( freeList ) repeaterClient ( from );
        if ( ! pNewClient ) {
            fprintf ( stderr, "%s: no memory for new client\n", __FILE__ );
            return;
        }
        if ( ! pNewClient->connect () ) {
            pNewClient->~repeaterClient ();
            freeList.release ( pNewClient );
            return;
        }
        client_list.add ( *pNewClient ); 
        newClient = true;
    }

    if ( ! pNewClient->sendConfirm () ) {
        client_list.remove ( *pNewClient );
        pNewClient->~repeaterClient ();
        freeList.release ( pNewClient );
#       ifdef DEBUG
            epicsUInt16 port = ntohs ( from.ia.sin_port );
            debugPrintf ( ( "Deleted repeater client=%u (error while sending ack)\n",
                        port ) );
#       endif
    }

    /*
     * send a noop message to all other clients so that we dont 
     * accumulate sockets when there are no beacons
     */
    caHdr noop;
    memset ( (char *) &noop, '\0', sizeof ( noop ) );
    AlignedWireRef < epicsUInt16 > ( noop.m_cmmd ) = CA_PROTO_VERSION;
    fanOut ( from, &noop, sizeof ( noop ), freeList );

    if ( newClient ) {
        /*
         * For HPUX and Solaris we need to verify that the clients
         * have not gone away - because an ICMP error return does not
         * get through to send(), which returns no error code.
         *
         * This is done each time that a new client is created.
         * See also the note in the file header.
         *
         * This is done here in order to avoid deleting a client
         * prior to sending its confirm message.
         */
        verifyClients ( freeList );
    }
}


/*
 *  ca_repeater ()
 */
void ca_repeater () 
{
    tsFreeList < repeaterClient, 0x20 > freeList;
    int size;
    SOCKET sock;
    osiSockAddr from;
    unsigned short port;
    char * pBuf; 

    pBuf = new char [MAX_UDP_RECV];

    {
        bool success = osiSockAttach();
        assert ( success );
    }

    port = envGetInetPortConfigParam ( & EPICS_CA_REPEATER_PORT,
                                       static_cast <unsigned short> (CA_REPEATER_PORT) );
    if ( ! makeSocket ( port, true, & sock ) ) {
        /*
         * test for server was already started
         */
        if ( SOCKERRNO == SOCK_EADDRINUSE ) {
            osiSockRelease ();
            debugPrintf ( ( "CA Repeater: exiting because a repeater is already running\n" ) );
            return;
        }
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf ( stderr, "%s: Unable to create repeater socket because \"%s\" - fatal\n",
            __FILE__, sockErrBuf );
        osiSockRelease ();
        delete [] pBuf;
        return;
    }

    debugPrintf ( ( "CA Repeater: Attached and initialized\n" ) );

    while ( true ) {
        osiSocklen_t from_size = sizeof ( from );
        size = recvfrom ( sock, pBuf, MAX_UDP_RECV, 0,
                    &from.sa, &from_size );
        if ( size < 0 ) {
            int errnoCpy = SOCKERRNO;
            // Avoid spurious ECONNREFUSED bug in linux
            if ( errnoCpy == SOCK_ECONNREFUSED ) {
                continue;
            }
            // Avoid ECONNRESET from connected socket in windows
            if ( errnoCpy == SOCK_ECONNRESET ) {
                continue;
            }
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            fprintf ( stderr, "CA Repeater: unexpected UDP recv err: %s\n",
                sockErrBuf );
            continue;
        }

        caHdr * pMsg = ( caHdr * ) pBuf;

        /*
         * both zero length message and a registration message
         * will register a new client
         */
        if ( ( (size_t) size) >= sizeof (*pMsg) ) {
            if ( AlignedWireRef < epicsUInt16 > ( pMsg->m_cmmd ) == REPEATER_REGISTER ) {
                register_new_client ( from, freeList );

                /*
                 * strip register client message
                 */
                pMsg++;
                size -= sizeof ( *pMsg );
                if ( size==0 ) {
                    continue;
                }
            }
            else if ( AlignedWireRef < epicsUInt16 > ( pMsg->m_cmmd ) == CA_PROTO_RSRV_IS_UP ) {
                if ( pMsg->m_available == 0u ) {
                    pMsg->m_available = from.ia.sin_addr.s_addr;
                }
            }
        }
        else if ( size == 0 ) {
            register_new_client ( from, freeList );
            continue;
        }

        fanOut ( from, pMsg, size, freeList ); 
    }
}

/*
 * caRepeaterThread ()
 */
extern "C" void caRepeaterThread ( void * /* pDummy */ )
{
    taskwdInsert ( epicsThreadGetIdSelf(), NULL, NULL );
    ca_repeater ();
}


