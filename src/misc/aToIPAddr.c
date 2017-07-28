/*************************************************************************\
* Copyright (c) 2013 LANS LLC, as Operator of
*     Los Alamos National Laboratory.
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * rational replacement for inet_addr()
 * 
 * author: Jeff Hill
 */
#include <stdio.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "epicsTypes.h"
#include "osiSock.h"

#ifndef NELEMENTS
#define NELEMENTS(A) (sizeof(A)/sizeof(A[0]))
#endif /*NELEMENTS*/

/*
 * addrArrayToUL ()
 */
static int addrArrayToUL ( const unsigned *pAddr, 
                          unsigned nElements, struct in_addr *pIpAddr )
{
    unsigned i;
    epicsUInt32 addr = 0ul;

    for ( i=0u; i < nElements; i++ ) {
        if ( pAddr[i] > 0xff ) {
            return -1;
        }
        addr <<= 8;
        addr |= ( epicsUInt32 ) pAddr[i];
    }
    pIpAddr->s_addr = htonl ( addr );
        
    return 0;
}

/*
 * initIPAddr()
 * !! ipAddr should be passed in in network byte order !!
 * !! port is passed in in host byte order !!
 */
static int initIPAddr ( struct in_addr ipAddr, unsigned port, 
                        struct sockaddr_in *pIP )
{
    if ( port > 0xffff ) {
        return -1;
    }
    {
        epicsUInt16 port_16 = (epicsUInt16) port;
        memset (pIP, '\0', sizeof(*pIP));
        pIP->sin_family = AF_INET;
        pIP->sin_port = htons(port_16);
        pIP->sin_addr = ipAddr;
    }
    return 0;
}

/*
 * rational replacement for inet_addr()
 * which allows the limited broadcast address
 * 255.255.255.255, allows the user
 * to specify a port number, and allows also a
 * named host to be specified.
 *
 * Sets the port number to "defaultPort" only if 
 * "pAddrString" does not contain an address of the form
 * "n.n.n.n:p or host:p"
 */
epicsShareFunc int epicsShareAPI 
aToIPAddr( const char *pAddrString, unsigned short defaultPort, 
                struct sockaddr_in *pIP )
{
    int status;
    unsigned addr[4];
    unsigned long rawAddr;
    /* 
     * !! change n elements here requires change in format below !! 
     */
    char hostName[512]; 
    char dummy[8]; 
    unsigned port;
    struct in_addr ina;

    /*
     * dotted ip addresses
     */
    status = sscanf ( pAddrString, " %u . %u . %u . %u %7s ", 
            addr, addr+1u, addr+2u, addr+3u, dummy );
    if ( status == 4 ) {
        if ( addrArrayToUL ( addr, NELEMENTS ( addr ), & ina ) < 0 ) {
            return -1;
        }
        port = defaultPort;
        return initIPAddr ( ina, port, pIP );
    }
    
    /*
     * dotted ip addresses and port
     */
    status = sscanf ( pAddrString, " %u . %u . %u . %u : %u %7s", 
            addr, addr+1u, addr+2u, addr+3u, &port, dummy );
    if ( status >= 5 ) {
        if ( status > 5 ) {
            /*
             * valid at the start but detritus on the end
             */
            return -1;
        }
        if ( addrArrayToUL ( addr, NELEMENTS ( addr ), &ina ) < 0 ) {
            return -1;
        }
        return initIPAddr ( ina, port, pIP );
    }

    /*
     * IP address as a raw number
     */
    status = sscanf ( pAddrString, " %lu %7s ", &rawAddr, dummy );
    if ( status == 1 ) {
        if ( rawAddr > 0xffffffff ) {
            return -1;
        }
        port = defaultPort;
        {
            epicsUInt32 rawAddr_32 = ( epicsUInt32 ) rawAddr;
            ina.s_addr = htonl ( rawAddr_32 );
            return initIPAddr ( ina, port, pIP );
        }
    }
    
    /*
     * IP address as a raw number, and port
     */
    status = sscanf ( pAddrString, " %lu : %u %7s ", &rawAddr, &port, dummy );
    if ( status >= 2 ) {
        if ( status > 2 ) {
            /*
             * valid at the start but detritus on the end
             */
            return -1;
        }
        if ( rawAddr > 0xffffffff ) {
            return -1;
        }
        {
            epicsUInt32 rawAddr_32 = ( epicsUInt32 ) rawAddr;
            ina.s_addr = htonl ( rawAddr_32 );
            return initIPAddr ( ina, port, pIP );
        }
    }


    /*
     * host name string 
     */
    status = sscanf ( pAddrString, " %511[^:] %s ", hostName, dummy );
    if ( status == 1 ) {
        port = defaultPort;
        status = hostToIPAddr ( hostName, &ina );
        if ( status == 0 ) {
            return initIPAddr ( ina, port, pIP );
        }
    }
    
    /*
     * host name string, and port
     */
    status = sscanf ( pAddrString, " %511[^:] : %u %s ", hostName, 
                        &port, dummy );
    if ( status >= 2 ) {
        if ( status > 2 ) {
            /*
             * valid at the start but detritus on the end
             */
            return -1;
        }
        status = hostToIPAddr ( hostName, &ina );
        if ( status == 0 ) {
            return initIPAddr ( ina, port, pIP );
        }
    }

    return -1;
}
