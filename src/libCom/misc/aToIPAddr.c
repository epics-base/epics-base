/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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
#include "osiSock.h"
#include "epicsStdlib.h"

/*
 * initIPAddr()
 * !! ipAddr should be passed in in network byte order !!
 * !! port is passed in in host byte order !!
 */
static int initIPAddr (struct in_addr ipAddr, unsigned short port,
    struct sockaddr_in *pIP)
{
    memset(pIP, '\0', sizeof(*pIP));
    pIP->sin_family = AF_INET;
    pIP->sin_port = htons(port);
    pIP->sin_addr = ipAddr;
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
 * "n.n.n.n:p"
 */
epicsShareFunc int epicsShareAPI
aToIPAddr(const char *pAddrString, unsigned short defaultPort,
    struct sockaddr_in *pIP)
{
    int status;
    char hostName[512]; /* !! change n elements here requires change in format below !! */
    char *endp;
    unsigned int port;
    unsigned long numaddr;
    struct in_addr ina;

    /*
     * Scan for a port number
     */
    status = sscanf( pAddrString, " %511[^:]:%u", hostName, &port );
    if ( status == 0 ) {
        return -1;
    }
    if ( status == 1 ) {
        port = defaultPort;
    }
    else if (status == 2 && port > 65535) {
        return -1;
    }

    /*
     * Look for a valid host name or dotted quad
     */
    status = hostToIPAddr( hostName, &ina );
    if ( status == 0 ) {
        return initIPAddr( ina, port, pIP );
    }

    /*
     * Try the IP address as a decimal integer
     */
    numaddr = strtoul( hostName, &endp, 10 );
    if (*endp)
        return -1;

    ina.s_addr = htonl( numaddr );
    return initIPAddr( ina, port, pIP );
}
