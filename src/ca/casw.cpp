
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

#include "iocinf.h"
#include "bhe_IL.h"
#include "inetAddrID_IL.h"

int main ( int argc, char **argv )
{
    osiTime programBeginTime = osiTime::getCurrent ();
    SOCKET sock;
    osiSockAddr addr;
    osiSocklen_t addrSize;
    char buf [0x4000];
    const char *pCurBuf;
    const caHdr *pCurMsg;
    unsigned serverPort;
    unsigned repeaterPort;
    int status;

    serverPort =
        envGetInetPortConfigParam ( &EPICS_CA_SERVER_PORT, CA_SERVER_PORT );

    repeaterPort =
        envGetInetPortConfigParam ( &EPICS_CA_REPEATER_PORT, CA_REPEATER_PORT );

    caStartRepeaterIfNotInstalled ( repeaterPort );

    sock = socket ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( sock == INVALID_SOCKET ) {
        printf ("casw: unable to create datagram socket because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        return -1;
    }

    memset ( (char *) &addr, 0 , sizeof (addr) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl (INADDR_ANY); 
    addr.ia.sin_port = htons ( 0 ); // any port
    status = bind ( sock, &addr.sa, sizeof (addr) );
    if ( status < 0 ) {
        socket_close ( sock );
        printf ("casw: unable to bind to an unconstrained address because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        return -1;
    }

    osiSockIoctl_t yes = true;
    status = socket_ioctl ( sock, FIONBIO, &yes );
    if ( status < 0 ) {
        socket_close ( sock );
        printf ("casw: unable to set socket to nonblocking state because \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        return -1;
    }

    unsigned attemptNumber = 0u;
    while ( true ) {
        caRepeaterRegistrationMessage ( sock, repeaterPort, attemptNumber );
        threadSleep ( 0.1 );
        addrSize = ( osiSocklen_t ) sizeof ( addr );
        status = recvfrom ( sock, buf, sizeof ( buf ), 0,
                            &addr.sa, &addrSize );
        if ( status >= static_cast <int> ( sizeof ( *pCurMsg ) ) ) {
            pCurMsg = reinterpret_cast < caHdr * > ( buf );
            if ( htons ( pCurMsg->m_cmmd ) == REPEATER_CONFIRM ) {
                break;
            }
        }

        attemptNumber++;
        if ( attemptNumber > 100 ) {
            socket_close ( sock );
            printf ( "casw: unable to register with the CA repeater\n" );
            return -1;
        }
    }

    osiSockIoctl_t no = false;
    status = socket_ioctl ( sock, FIONBIO, &no );
    if ( status < 0 ) {
        socket_close ( sock );
        printf ("casw: unable to set socket to blocking state because \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        return -1;
    }

    resTable < bhe, inetAddrID > beaconTable ( 1024 );
    while ( 1 ) {

        addrSize = ( osiSocklen_t ) sizeof ( addr );
        status = recvfrom ( sock, buf, sizeof ( buf ), 0,
                            &addr.sa, &addrSize );
        if ( status <= 0 ) {
            socket_close ( sock );
            printf ("casw: error from recv was = \"%s\"\n",
                SOCKERRSTR (SOCKERRNO));
            return -1;
        }

        if ( addr.sa.sa_family != AF_INET ) {
            continue;
        }
        
        pCurMsg = reinterpret_cast < const caHdr * > ( ( pCurBuf = buf ) );
        while ( status >= static_cast < int > ( sizeof ( *pCurMsg ) ) ) {
            if ( ntohs ( pCurMsg->m_cmmd ) == CA_PROTO_RSRV_IS_UP ) {
                struct sockaddr_in ina;

                /* 
                 * this allows a fan-out server to potentially
                 * insert the true address of the CA server 
                 *
                 * old servers:
                 *   1) set this field to one of the ip addresses of the host _or_
                 *   2) set this field to htonl(INADDR_ANY)
                 * new servers:
                 *   always set this field to htonl(INADDR_ANY)
                 *
                 * clients always assume that if this
                 * field is set to something that isnt htonl(INADDR_ANY)
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
                    ina.sin_port = ntohs ( serverPort );
                }

                bool netChange;

                /*
                 * look for it in the hash table
                 */
                bhe *pBHE = beaconTable.lookup ( ina );
                if ( pBHE ) {
                    netChange = pBHE->updateBeaconPeriod ( programBeginTime );
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
                    pBHE = new bhe ( osiTime::getCurrent (), ina );
                    if ( pBHE ) {
                        if ( beaconTable.add ( *pBHE ) < 0 ) {
                            pBHE->destroy ();
                        }
                    }
                }

                if ( netChange ) {
                    char date[64];
                    osiTime current = osiTime::getCurrent ();
                    current.strftime ( date, sizeof ( date ), "%a %b %d %H:%M:%S %Y");
                    char host[64];
                    ipAddrToA ( &ina, host, sizeof ( host ) );
                    printf ("CA server beacon anomaly: %s %s\n", date, host );
                }
            }
            pCurBuf += sizeof ( *pCurMsg ) + ntohl ( pCurMsg->m_postsize );
            pCurMsg = reinterpret_cast < const caHdr * > ( pCurBuf );
            status -= sizeof ( *pCurMsg ) + ntohl ( pCurMsg->m_postsize );
        }
    }
}
