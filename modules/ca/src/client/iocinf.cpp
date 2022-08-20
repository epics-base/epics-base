/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
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

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include <vector>
#include <exception>

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "envDefs.h"
#include "epicsAssert.h"
#include "epicsString.h"
#include "epicsStdioRedirect.h"
#include "errlog.h"
#include "osiWireFormat.h"

#include "addrList.h"
#include "iocinf.h"

/*
 * addAddrToChannelAccessAddressList ()
 */
extern "C" int epicsStdCall addAddrToChannelAccessAddressList
    ( ELLLIST *pList, const ENV_PARAM *pEnv,
    unsigned short port, int ignoreNonDefaultPort )
{
    osiSockAddrNode *pNewNode;
    const char *pStr;
    struct sockaddr_in addr;
    int status, ret = -1;

    pStr = envGetConfigParamPtr (pEnv);
    if (!pStr) {
        return ret;
    }

    try {
        std::vector<char> scratch(pStr, pStr+strlen(pStr)+1); // copy chars and trailing nil

        char *save = NULL;
        for(const char *pToken = epicsStrtok_r(&scratch[0], " \t\n\r", &save);
            pToken;
            pToken = epicsStrtok_r(NULL, " \t\n\r", &save))
        {
            if(!pToken[0]) {
                continue;
            }

            status = aToIPAddr ( pToken, port, &addr );
            if (status<0) {
                fprintf ( stderr, "%s: Parsing '%s'\n", __FILE__, pEnv->name);
                fprintf ( stderr, "\tBad internet address or host name: '%s'\n", pToken);
                continue;
            }

            if ( ignoreNonDefaultPort && ntohs ( addr.sin_port ) != port ) {
                continue;
            }

            pNewNode = (osiSockAddrNode *) calloc (1, sizeof(*pNewNode));
            if (pNewNode==NULL) {
                fprintf ( stderr, "addAddrToChannelAccessAddressList(): no memory available for configuration\n");
                break;
            }

            pNewNode->addr.ia = addr;

            /*
             * LOCK applied externally
             */
            ellAdd (pList, &pNewNode->node);
            ret = 0; /* success if anything is added to the list */
        }
    } catch(std::exception&) { // only bad_alloc currently possible
        ret = -1;
    }

    return ret;
}

/*
 * removeDuplicateAddresses ()
 */
extern "C" void epicsStdCall removeDuplicateAddresses 
    ( ELLLIST *pDestList, ELLLIST *pSrcList, int silent )
{
    ELLNODE *pRawNode;

    while ( (pRawNode  = ellGet ( pSrcList ) ) ) {
        STATIC_ASSERT ( offsetof (osiSockAddrNode, node) == 0 );
        osiSockAddrNode *pNode = reinterpret_cast <osiSockAddrNode *> ( pRawNode );
        osiSockAddrNode *pTmpNode;

        if ( pNode->addr.sa.sa_family == AF_INET ) {

            pTmpNode = (osiSockAddrNode *) ellFirst (pDestList);
            while ( pTmpNode ) {
                if (pTmpNode->addr.sa.sa_family == AF_INET) {
                    if ( pNode->addr.ia.sin_addr.s_addr == pTmpNode->addr.ia.sin_addr.s_addr &&
                            pNode->addr.ia.sin_port == pTmpNode->addr.ia.sin_port ) {
                        if ( ! silent ) {
                            char buf[64];
                            ipAddrToDottedIP ( &pNode->addr.ia, buf, sizeof (buf) );
                            fprintf ( stderr,
                                "Warning: Duplicate EPICS CA Address list entry \"%s\" discarded\n", buf );
                        }
                        free (pNode);
                        pNode = NULL;
                        break;
                    }
                }
                pTmpNode = (osiSockAddrNode *) ellNext (&pTmpNode->node);
            }
            if (pNode) {
                ellAdd (pDestList, &pNode->node);
            }
        }
        else {
            ellAdd (pDestList, &pNode->node);
        }
    }
}

/*
 * forcePort ()
 */
static void  forcePort ( ELLLIST *pList, unsigned short port )
{
    osiSockAddrNode *pNode;

    pNode  = ( osiSockAddrNode * ) ellFirst ( pList );
    while ( pNode ) {
        if ( pNode->addr.sa.sa_family == AF_INET ) {
            pNode->addr.ia.sin_port = htons ( port );
        }
        pNode = ( osiSockAddrNode * ) ellNext ( &pNode->node );
    }
}


/*
 * configureChannelAccessAddressList ()
 */
extern "C" void epicsStdCall configureChannelAccessAddressList 
        ( ELLLIST *pList, SOCKET sock, unsigned short port )
{
    ELLLIST         tmpList;
    char            *pstr;
    char            yesno[32u];
    int             yes;

    /*
     * don't load the list twice
     */
    assert ( ellCount (pList) == 0 );

    ellInit ( &tmpList );

    /*
     * Check to see if the user has disabled
     * initializing the search b-cast list
     * from the interfaces found.
     */
    yes = true;
    pstr = envGetConfigParam ( &EPICS_CA_AUTO_ADDR_LIST,
            sizeof (yesno), yesno );
    if ( pstr ) {
        if ( strstr ( pstr, "no" ) || strstr ( pstr, "NO" ) ) {
            yes = false;
        }
    }

    /*
     * LOCK is for piiu->destAddr list
     * (lock outside because this is used by the server also)
     */
    if (yes) {
        ELLLIST bcastList;
        osiSockAddr addr;
        ellInit ( &bcastList );
        addr.ia.sin_family = AF_UNSPEC;
        osiSockDiscoverBroadcastAddresses ( &bcastList, sock, &addr );
        forcePort ( &bcastList, port );
        removeDuplicateAddresses ( &tmpList, &bcastList, 1 );
        if ( ellCount ( &tmpList ) == 0 ) {
            osiSockAddrNode *pNewNode;
            pNewNode = (osiSockAddrNode *) calloc ( 1, sizeof (*pNewNode) );
            if ( pNewNode ) {
                /*
                 * if no interfaces found then look for local channels
                 * with the loop back interface
                 */
                pNewNode->addr.ia.sin_family = AF_INET;
                pNewNode->addr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK );
                pNewNode->addr.ia.sin_port = htons ( port );
                ellAdd ( &tmpList, &pNewNode->node );
            }
            else {
                errlogPrintf ( "configureChannelAccessAddressList(): no memory available for configuration\n" );
            }
        }
    }
    addAddrToChannelAccessAddressList ( &tmpList, &EPICS_CA_ADDR_LIST, port, false );

    removeDuplicateAddresses ( pList, &tmpList, 0 );
}


/*
 * printChannelAccessAddressList ()
 */
extern "C" void epicsStdCall printChannelAccessAddressList ( const ELLLIST *pList )
{
    osiSockAddrNode *pNode;

    ::printf ( "Channel Access Address List\n" );
    pNode = (osiSockAddrNode *) ellFirst ( pList );
    while (pNode) {
        char buf[64];
        ipAddrToA ( &pNode->addr.ia, buf, sizeof ( buf ) );
        ::printf ( "%s\n", buf );
        pNode = (osiSockAddrNode *) ellNext ( &pNode->node );
    }
}
