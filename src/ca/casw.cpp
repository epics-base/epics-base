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
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include <stdio.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "envDefs.h" 
#include "errlog.h"
#include "osiWireFormat.h"

#include "bhe.h"
#include "udpiiu.h"
#include "inetAddrID.h"

// using a wrapper class around the free list avoids
// Tornado 2.0.1 GNU compiler bugs
class bheFreeStoreMgr : public bheMemoryManager {
public:
    bheFreeStoreMgr () {}
    void * allocate ( size_t );
    void release ( void * );
private:
    tsFreeList < class bhe, 0x100 > freeList;
	bheFreeStoreMgr ( const bheFreeStoreMgr & );
	bheFreeStoreMgr & operator = ( const bheFreeStoreMgr & );
};

void * bheFreeStoreMgr::allocate ( size_t size )
{
    return freeList.allocate ( size );
}

void bheFreeStoreMgr::release ( void * pCadaver )
{
    freeList.release ( pCadaver );
}

int main ( int argc, char ** argv )
{
    epicsMutex mutex;
    epicsGuard < epicsMutex > guard ( mutex );
    bheFreeStoreMgr bheFreeList;
    epicsTime programBeginTime = epicsTime::getCurrent ();
    bool validCommandLine = false;
    unsigned interest = 0u;
    SOCKET sock;
    osiSockAddr addr;
    osiSocklen_t addrSize;
    char buf [0x4000];
    const char *pCurBuf;
    const caHdr *pCurMsg;
    ca_uint16_t serverPort;
    ca_uint16_t repeaterPort;
    int status;

    if ( argc == 1 ) {
        validCommandLine = true;
    }
    else if ( argc == 2 ) {
        status = sscanf ( argv[1], " -i%u ", & interest );
        if ( status == 1 ) {
            validCommandLine = true;
        }
    }
    else if ( argc == 3 ) {
        if ( strcmp ( argv[1], "-i" ) == 0 ) {
            status = sscanf ( argv[2], " %u ", & interest );
            if ( status == 1 ) {
                validCommandLine = true;
            }
        }
    }

    if ( ! validCommandLine ) {
        printf ( "usage: casw <-i interestLevel>\n" );
        return 0;
    }

    serverPort =
        envGetInetPortConfigParam ( &EPICS_CA_SERVER_PORT,
                                    static_cast <unsigned short> (CA_SERVER_PORT) );

    repeaterPort =
        envGetInetPortConfigParam ( &EPICS_CA_REPEATER_PORT,
                                    static_cast <unsigned short> (CA_REPEATER_PORT) );

    caStartRepeaterIfNotInstalled ( repeaterPort );

    sock = epicsSocketCreate ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( sock == INVALID_SOCKET ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ("casw: unable to create datagram socket because = \"%s\"\n",
            sockErrBuf );
        return -1;
    }

    memset ( (char *) &addr, 0 , sizeof (addr) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl ( INADDR_ANY );
    addr.ia.sin_port = htons ( 0 );  // any port
    status = bind ( sock, &addr.sa, sizeof (addr) );
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        epicsSocketDestroy ( sock );
        errlogPrintf ( "casw: unable to bind to an unconstrained address because = \"%s\"\n",
            sockErrBuf );
        return -1;
    }

    osiSockIoctl_t yes = true;
    status = socket_ioctl ( sock, FIONBIO, &yes );
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        epicsSocketDestroy ( sock );
        errlogPrintf ( "casw: unable to set socket to nonblocking state because \"%s\"\n",
            sockErrBuf );
        return -1;
    }

    unsigned attemptNumber = 0u;
    while ( true ) {
        caRepeaterRegistrationMessage ( sock, repeaterPort, attemptNumber );
        epicsThreadSleep ( 0.1 );
        addrSize = ( osiSocklen_t ) sizeof ( addr );
        status = recvfrom ( sock, buf, sizeof ( buf ), 0,
                            &addr.sa, &addrSize );
        if ( status >= static_cast <int> ( sizeof ( *pCurMsg ) ) ) {
            pCurMsg = reinterpret_cast < caHdr * > ( buf );
            epicsUInt16 cmmd = AlignedWireRef < const epicsUInt16 > ( pCurMsg->m_cmmd );
            if ( cmmd == REPEATER_CONFIRM ) {
                break;
            }
        }

        attemptNumber++;
        if ( attemptNumber > 100 ) {
            epicsSocketDestroy ( sock );
            errlogPrintf ( "casw: unable to register with the CA repeater\n" );
            return -1;
        }
    }

    osiSockIoctl_t no = false;
    status = socket_ioctl ( sock, FIONBIO, &no );
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        epicsSocketDestroy ( sock );
        errlogPrintf ( "casw: unable to set socket to blocking state because \"%s\"\n",
            sockErrBuf );
        return -1;
    }

    resTable < bhe, inetAddrID > beaconTable;
    while ( true ) {

        addrSize = ( osiSocklen_t ) sizeof ( addr );
        status = recvfrom ( sock, buf, sizeof ( buf ), 0,
                            &addr.sa, &addrSize );
        if ( status <= 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            epicsSocketDestroy ( sock );
            errlogPrintf ("casw: error from recv was = \"%s\"\n",
                sockErrBuf );
            return -1;
        }

        if ( addr.sa.sa_family != AF_INET ) {
            continue;
        }
        
        unsigned byteCount = static_cast <unsigned> ( status );
        pCurMsg = reinterpret_cast < const caHdr * > ( ( pCurBuf = buf ) );
        while ( byteCount ) {
            AlignedWireRef < const epicsUInt16 > pstSize ( pCurMsg->m_postsize );
            size_t msgSize = pstSize + sizeof ( *pCurMsg ) ;
            if ( msgSize > byteCount ) {
                errlogPrintf ( "CASW: udp input protocol violation\n" );
                break;
            }

            epicsUInt16 cmmd = AlignedWireRef < const epicsUInt16 > ( pCurMsg->m_cmmd );
            if ( cmmd == CA_PROTO_RSRV_IS_UP ) {
                bool anomaly = false;
                epicsTime previousTime;
                struct sockaddr_in ina;

                /* 
                 * this allows a fan-out server to potentially
                 * insert the true address of the CA server 
                 *
                 * old servers:
                 *   1) set this field to one of the ip addresses of the host _or_
                 *   2) set this field to INADDR_ANY
                 * new servers:
                 *   always set this field to INADDR_ANY
                 *
                 * clients always assume that if this
                 * field is set to something that isnt INADDR_ANY
                 * then it is the overriding IP address of the server.
                 */
                ina.sin_family = AF_INET;
                ina.sin_addr.s_addr = pCurMsg->m_available;

                if ( pCurMsg->m_count != 0 ) {
                    ina.sin_port = pCurMsg->m_count;
                }
                else {
                    /*
                     * old servers dont supply this and the
                     * default port must be assumed
                     */
                    ina.sin_port = htons ( serverPort );
                }

                ca_uint32_t beaconNumber = ntohl ( pCurMsg->m_cid );
                unsigned protocolRevision = ntohs ( pCurMsg->m_dataType );

                epicsTime currentTime = epicsTime::getCurrent();

                /*
                 * look for it in the hash table
                 */
                bhe *pBHE = beaconTable.lookup ( ina );
                if ( pBHE ) {
                    previousTime = pBHE->updateTime ( guard );
                    anomaly = pBHE->updatePeriod ( 
                        guard, programBeginTime, 
                        currentTime, beaconNumber, protocolRevision );
                }
                else {
                    /*
                     * This is the first beacon seen from this server.
                     * Wait until 2nd beacon is seen before deciding
                     * if it is a new server (or just the first
                     * time that we have seen a server's beacon
                     * shortly after the program started up)
                     */
                    pBHE = new ( bheFreeList ) 
                        bhe ( mutex, currentTime, beaconNumber, ina );
                    if ( pBHE ) {
                        if ( beaconTable.add ( *pBHE ) < 0 ) {
                            pBHE->~bhe ();
                            bheFreeList.release ( pBHE );
                        }
                    }
                }
                if ( anomaly || interest > 1 ) {
                    char date[64];
                    currentTime.strftime ( date, sizeof ( date ), 
                        "%Y-%m-%d %H:%M:%S.%09f");
                    char host[64];
                    ipAddrToA ( &ina, host, sizeof ( host ) );
                    const char * pPrefix = "";
                    if ( interest > 1 ) {
                        if ( anomaly ) {
                            pPrefix = "* ";
                        }
                        else {
                            pPrefix = "  ";
                        }
                    }
                    printf ( "%s%-40s %s\n", 
                        pPrefix, host, date );
                    if ( anomaly && interest > 0 ) {
                        printf ( "\testimate=%f current=%f\n", 
                            pBHE->period ( guard ), 
                            currentTime - previousTime );
                    }
                    fflush(stdout);
                }
            }
            pCurBuf += msgSize;
            pCurMsg = reinterpret_cast < const caHdr * > ( pCurBuf );
            byteCount -= msgSize;
        }
    }
}
