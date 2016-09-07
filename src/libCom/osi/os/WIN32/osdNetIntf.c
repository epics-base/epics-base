/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *      WIN32 specific initialisation for bsd sockets,
 *      based on Chris Timossi's base/src/ca/windows_depend.c,
 *      and also further additions by Kay Kasemir when this was in
 *      dllmain.cc
 *
 *      7-1-97  -joh-
 *
 */

/*
 * ANSI C
 */
#include <stdio.h>
#include <stdlib.h>

/*
 * WIN32 specific
 */
#define VC_EXTRALEAN
#define STRICT
#include <winsock2.h>
#include <ws2tcpip.h>

/*
 * EPICS
 */
#define epicsExportSharedSymbols
#include "osiSock.h"
#include "errlog.h"
#include "epicsThread.h"
#include "epicsVersion.h"

static osiSockAddr      osiLocalAddrResult;
static epicsThreadOnceId osiLocalAddrId = EPICS_THREAD_ONCE_INIT;

/*
 * osiLocalAddr ()
 */
static void osiLocalAddrOnce ( void *raw )
{
    SOCKET              *psocket = raw;
    osiSockAddr         addr;
    int                 status;
    INTERFACE_INFO      *pIfinfo;
    INTERFACE_INFO      *pIfinfoList = NULL;
    unsigned            nelem;
    DWORD               numifs;
    DWORD               cbBytesReturned;

    memset ( (void *) &addr, '\0', sizeof ( addr ) );
    addr.sa.sa_family = AF_UNSPEC;

    /* only valid for winsock 2 and above */
    if ( wsaMajorVersion() < 2 ) {
        goto fail;
    }

    nelem = 100;
    pIfinfoList = (INTERFACE_INFO *) calloc ( nelem, sizeof (INTERFACE_INFO) );
    if (!pIfinfoList) {
        errlogPrintf ("calloc failed\n");
        goto fail;
    }

    status = WSAIoctl (*psocket, SIO_GET_INTERFACE_LIST, NULL, 0,
                       (LPVOID)pIfinfoList, nelem*sizeof(INTERFACE_INFO),
                       &cbBytesReturned, NULL, NULL);

    if (status != 0 || cbBytesReturned == 0) {
        errlogPrintf ("WSAIoctl SIO_GET_INTERFACE_LIST failed %d\n",WSAGetLastError());
        goto fail;
    }

    numifs = cbBytesReturned / sizeof(INTERFACE_INFO);
    for (pIfinfo = pIfinfoList; pIfinfo < (pIfinfoList+numifs); pIfinfo++){

        /*
         * dont use interfaces that have been disabled
         */
        if (!(pIfinfo->iiFlags & IFF_UP)) {
            continue;
        }

        /*
         * dont use the loop back interface
         */
        if (pIfinfo->iiFlags & IFF_LOOPBACK) {
            continue;
        }

        addr.sa = pIfinfo->iiAddress.Address;

        /* Work around MS Winsock2 bugs */
        if (addr.sa.sa_family == 0) {
            addr.sa.sa_family = AF_INET;
        }

        osiLocalAddrResult = addr;
        free ( pIfinfoList );
        return;
    }

    errlogPrintf (
                "osiLocalAddr(): only loopback found\n");
fail:
    /* fallback to loopback */
    memset ( (void *) &addr, '\0', sizeof ( addr ) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    osiLocalAddrResult = addr;

    free ( pIfinfoList );
}

epicsShareFunc osiSockAddr epicsShareAPI osiLocalAddr (SOCKET socket)
{
    epicsThreadOnce(&osiLocalAddrId, osiLocalAddrOnce, (void*)&socket);
    return osiLocalAddrResult;
}

/*
 * osiSockDiscoverBroadcastAddresses ()
 */
epicsShareFunc void epicsShareAPI osiSockDiscoverBroadcastAddresses
     (ELLLIST *pList, SOCKET socket, const osiSockAddr *pMatchAddr)
{
    int                 status;
    INTERFACE_INFO      *pIfinfo;
    INTERFACE_INFO      *pIfinfoList;
    unsigned            nelem;
    int                 numifs;
    DWORD               cbBytesReturned;
    osiSockAddrNode     *pNewNode;

    if ( pMatchAddr->sa.sa_family == AF_INET  ) {
        if ( pMatchAddr->ia.sin_addr.s_addr == htonl (INADDR_LOOPBACK) ) {
            pNewNode = (osiSockAddrNode *) calloc (1, sizeof (*pNewNode) );
            if ( pNewNode == NULL ) {
                return;
            }
            pNewNode->addr.ia.sin_family = AF_INET;
            pNewNode->addr.ia.sin_port = htons ( 0 );
            pNewNode->addr.ia.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
            ellAdd ( pList, &pNewNode->node );
            return;
        }
    }

    /* only valid for winsock 2 and above */
    if (wsaMajorVersion() < 2 ) {
        fprintf(stderr, "Need to set EPICS_CA_AUTO_ADDR_LIST=NO for winsock 1\n");
        return;
    }

    nelem = 100;
    pIfinfoList = (INTERFACE_INFO *) calloc(nelem, sizeof(INTERFACE_INFO));
    if(!pIfinfoList){
        return;
    }

    status = WSAIoctl (socket, SIO_GET_INTERFACE_LIST,
                       NULL, 0,
                       (LPVOID)pIfinfoList, nelem*sizeof(INTERFACE_INFO),
                       &cbBytesReturned, NULL, NULL);

    if (status != 0 || cbBytesReturned == 0) {
        fprintf(stderr, "WSAIoctl SIO_GET_INTERFACE_LIST failed %d\n",WSAGetLastError());
        free(pIfinfoList);
        return;
    }

    numifs = cbBytesReturned/sizeof(INTERFACE_INFO);
    for (pIfinfo = pIfinfoList; pIfinfo < (pIfinfoList+numifs); pIfinfo++){

        /*
         * dont bother with interfaces that have been disabled
         */
        if (!(pIfinfo->iiFlags & IFF_UP)) {
            continue;
        }

        if (pIfinfo->iiFlags & IFF_LOOPBACK) {
            continue;
        }

        /*
         * work around WS2 bug
         */
        if (pIfinfo->iiAddress.Address.sa_family != AF_INET) {
            if (pIfinfo->iiAddress.Address.sa_family == 0) {
                pIfinfo->iiAddress.Address.sa_family = AF_INET;
            }
        }

        /*
         * if it isnt a wildcarded interface then look for
         * an exact match
         */
        if (pMatchAddr->sa.sa_family != AF_UNSPEC) {
            if (pIfinfo->iiAddress.Address.sa_family != pMatchAddr->sa.sa_family) {
                continue;
            }
            if (pIfinfo->iiAddress.Address.sa_family != AF_INET) {
                continue;
            }
            if (pMatchAddr->sa.sa_family != AF_INET) {
                continue;
            }
            if (pMatchAddr->ia.sin_addr.s_addr != htonl(INADDR_ANY)) {
                if (pIfinfo->iiAddress.AddressIn.sin_addr.s_addr != pMatchAddr->ia.sin_addr.s_addr) {
                    continue;
                }
            }
        }

        pNewNode = (osiSockAddrNode *) calloc (1, sizeof(*pNewNode));
        if (pNewNode==NULL) {
            errlogPrintf ("osiSockDiscoverBroadcastAddresses(): no memory available for configuration\n");
            return;
        }

        if (pIfinfo->iiAddress.Address.sa_family == AF_INET &&
                pIfinfo->iiFlags & IFF_BROADCAST) {
            const unsigned mask = pIfinfo->iiNetmask.AddressIn.sin_addr.s_addr;
            const unsigned bcast = pIfinfo->iiBroadcastAddress.AddressIn.sin_addr.s_addr;
            const unsigned addr = pIfinfo->iiAddress.AddressIn.sin_addr.s_addr;
            unsigned result = (addr & mask) | (bcast &~mask);
            pNewNode->addr.ia.sin_family = AF_INET;
            pNewNode->addr.ia.sin_addr.s_addr = result;
            pNewNode->addr.ia.sin_port = htons ( 0 );
        }
        else {
            pNewNode->addr.sa = pIfinfo->iiBroadcastAddress.Address;
        }

        /*
         * LOCK applied externally
         */
        ellAdd (pList, &pNewNode->node);
    }

    free (pIfinfoList);
}
