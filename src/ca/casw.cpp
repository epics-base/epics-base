
/* * $Id$
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

int main ( int, char ** )
{
    epicsTime programBeginTime = epicsTime::getCurrent ();
    SOCKET sock;
    osiSockAddr addr;
    osiSocklen_t addrSize;
    char buf [0x4000];
    const char *pCurBuf;
    const caHdr *pCurMsg;
    ca_uint16_t serverPort;
    ca_uint16_t repeaterPort;
    int status;

    serverPort =
        envGetInetPortConfigParam ( &EPICS_CA_SERVER_PORT,
                                    static_cast <unsigned short> (CA_SERVER_PORT) );

    repeaterPort =
        envGetInetPortConfigParam ( &EPICS_CA_REPEATER_PORT,
                                    static_cast <unsigned short> (CA_REPEATER_PORT) );

    caStartRepeaterIfNotInstalled ( repeaterPort );

    sock = socket ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( sock == INVALID_SOCKET ) {
        errlogPrintf ("casw: unable to create datagram socket because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        return -1;
    }

    memset ( (char *) &addr, 0 , sizeof (addr) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = epicsHTON32 ( INADDR_ANY ); 
    addr.ia.sin_port = epicsHTON16 ( 0 ); // any port
    status = bind ( sock, &addr.sa, sizeof (addr) );
    if ( status < 0 ) {
        socket_close ( sock );
        errlogPrintf ("casw: unable to bind to an unconstrained address because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        return -1;
    }

    osiSockIoctl_t yes = true;
    status = socket_ioctl ( sock, FIONBIO, &yes ); // X aCC 392
    if ( status < 0 ) {
        socket_close ( sock );
        errlogPrintf ("casw: unable to set socket to nonblocking state because \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
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
            if ( epicsHTON16 ( pCurMsg->m_cmmd ) == REPEATER_CONFIRM ) {
                break;
            }
        }

        attemptNumber++;
        if ( attemptNumber > 100 ) {
            socket_close ( sock );
            errlogPrintf ( "casw: unable to register with the CA repeater\n" );
            return -1;
        }
    }

    osiSockIoctl_t no = false;
    status = socket_ioctl ( sock, FIONBIO, &no ); // X aCC 392
    if ( status < 0 ) {
        socket_close ( sock );
        errlogPrintf ("casw: unable to set socket to blocking state because \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        return -1;
    }

    resTable < bhe, inetAddrID > beaconTable;
    while ( true ) {

        addrSize = ( osiSocklen_t ) sizeof ( addr );
        status = recvfrom ( sock, buf, sizeof ( buf ), 0,
                            &addr.sa, &addrSize );
        if ( status <= 0 ) {
            socket_close ( sock );
            errlogPrintf ("casw: error from recv was = \"%s\"\n",
                SOCKERRSTR (SOCKERRNO));
            return -1;
        }

        if ( addr.sa.sa_family != AF_INET ) {
            continue;
        }
        
        pCurMsg = reinterpret_cast < const caHdr * > ( ( pCurBuf = buf ) );
        while ( status >= static_cast < int > ( sizeof ( *pCurMsg ) ) ) {
            if ( epicsNTOH16 ( pCurMsg->m_cmmd ) == CA_PROTO_RSRV_IS_UP ) {
                struct sockaddr_in ina;

                /* 
                 * this allows a fan-out server to potentially
                 * insert the true address of the CA server 
                 *
                 * old servers:
                 *   1) set this field to one of the ip addresses of the host _or_
                 *   2) set this field to epicsHTON32(INADDR_ANY)
                 * new servers:
                 *   always set this field to epicsHTON32(INADDR_ANY)
                 *
                 * clients always assume that if this
                 * field is set to something that isnt epicsHTON32(INADDR_ANY)
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
                    ina.sin_port = epicsNTOH16 ( serverPort );
                }

                bool netChange;
                epicsTime currentTime = epicsTime::getCurrent();

                /*
                 * look for it in the hash table
                 */
                bhe *pBHE = beaconTable.lookup ( ina );
                if ( pBHE ) {
                    netChange = pBHE->updatePeriod ( programBeginTime, currentTime );
                }
                else {
                    /*
                     * This is the first beacon seen from this server.
                     * Wait until 2nd beacon is seen before deciding
                     * if it is a new server (or just the first
                     * time that we have seen a server's beacon
                     * shortly after the program started up)
                     */
                    netChange = false;
                    pBHE = new bhe ( currentTime, ina );
                    if ( pBHE ) {
                        if ( beaconTable.add ( *pBHE ) < 0 ) {
                            pBHE->destroy ();
                        }
                    }
                }

                if ( netChange ) {
                    char date[64];
                    currentTime.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S");
                    char host[64];
                    ipAddrToA ( &ina, host, sizeof ( host ) );
                    errlogPrintf ("CA server beacon anomaly: %s %s\n", date, host );
                }
            }
            pCurBuf += sizeof ( *pCurMsg ) + epicsNTOH32 ( pCurMsg->m_postsize );
            pCurMsg = reinterpret_cast < const caHdr * > ( pCurBuf );
            status -= sizeof ( *pCurMsg ) + epicsNTOH32 ( pCurMsg->m_postsize );
        }
    }
}
