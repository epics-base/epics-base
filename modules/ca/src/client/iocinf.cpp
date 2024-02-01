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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "envDefs.h"
#include "epicsAssert.h"
#include "epicsStdioRedirect.h"
#include "errlog.h"
#include "osiWireFormat.h"

#include "addrList.h"
#include "iocinf.h"

#include "osiSock.h" /* NETDEBUG */
#include "epicsSock.h"
#include "epicsNetDebugLog.h"
/*
 * getToken()
 */
static char *getToken ( const char **ppString, char *pBuf, unsigned bufSIze )
{
    bool tokenFound = false;
    const char *pToken;
    unsigned i;

    pToken = *ppString;
    while ( isspace (*pToken) && *pToken ){
        pToken++;
    }

    for ( i=0u; i<bufSIze; i++ ) {
        if ( isspace (pToken[i]) || pToken[i]=='\0' ) {
            pBuf[i] = '\0';
            *ppString = &pToken[i];
            if ( i != 0 ) {
                tokenFound = true;
            }
            break;
        }
        pBuf[i] = pToken[i];
    }

    if ( tokenFound ) {
        pBuf[bufSIze-1] = '\0';
        return pBuf;
    }
    return NULL;
}

/*
 * addAddrToChannelAccessAddressList ()
 */
extern "C" int epicsStdCall addAddrToChannelAccessAddressList
    ( ELLLIST *pList, const ENV_PARAM *pEnv,
    unsigned short port, int ignoreNonDefaultPort )
{
    osiSockAddrNode46 *pNewNode;
    const char *pStr;
    const char *pToken;
    osiSockAddr46 addr46;
    char buf[256u]; /* large enough to hold an IP address or hostname */
    int status, ret = -1;

    pStr = envGetConfigParamPtr (pEnv);
    if (!pStr) {
        return ret;
    }

    while ( ( pToken = getToken (&pStr, buf, sizeof (buf) ) ) ) {
      status = aToIPAddr46 ( pToken, port, &addr46);
        if (status<0) {
            fprintf ( stderr, "%s: Parsing '%s'\n", __FILE__, pEnv->name);
            fprintf ( stderr, "\tBad internet address or host name: '%s'\n", pToken);
            continue;
        }

        if ( ignoreNonDefaultPort && epicsSocket46portFromAddress(&addr46) != port ) {
#ifdef NETDEBUG
            char buf[64];
            sockAddrToDottedIP(&addr46.sa, buf, sizeof(buf));
            epicsNetDebugLog("NET addAddrToChannelAccessAddressList: ignore addr='%s' port=%u\n",
			      buf, port);
#endif
            continue;
        }
#ifdef NETDEBUG
        {
          char buf[64];
          sockAddrToDottedIP(&addr46.sa, buf, sizeof(buf));
          epicsNetDebugLog("NET addAddrToChannelAccessAddressList: add addr='%s'\n",
			    buf );
        }
#endif
        pNewNode = (osiSockAddrNode46 *) calloc (1, sizeof(*pNewNode));
        if (pNewNode==NULL) {
            fprintf ( stderr, "addAddrToChannelAccessAddressList(): no memory available for configuration\n");
            break;
        }

        pNewNode->addr = addr46;

        /*
         * LOCK applied externally
         */
        ellAdd (pList, &pNewNode->node);
        ret = 0; /* success if anything is added to the list */
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
        STATIC_ASSERT ( offsetof (osiSockAddrNode46, node) == 0 );
        osiSockAddrNode46 *pNode = reinterpret_cast <osiSockAddrNode46 *> ( pRawNode );
        osiSockAddrNode46 *pTmpNode;

        if ( epicsSocket46IsAF_INETorAF_INET6 (pNode->addr.sa.sa_family ) ) {

            pTmpNode = (osiSockAddrNode46 *) ellFirst (pDestList);
            while ( pTmpNode ) {
                if ( sockAddrAreIdentical46(&pTmpNode->addr, &pNode->addr ) ) {
                    if ( ! silent ) {
                        char buf[64];
                        sockAddrToDottedIP (&pNode->addr.sa, buf, sizeof (buf) );
                        fprintf ( stderr,
                                  "Warning: Duplicate EPICS CA Address list entry \"%s\" discarded\n", buf );
                    }
                    free (pNode);
                    pNode = NULL;
                    break;
                }
                pTmpNode = (osiSockAddrNode46 *) ellNext (&pTmpNode->node);
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
    osiSockAddrNode46 *pNode;

    pNode  = ( osiSockAddrNode46 * ) ellFirst ( pList );
    while ( pNode ) {
        if ( pNode->addr.sa.sa_family == AF_INET ) {
            pNode->addr.ia.sin_port = htons ( port );
#ifdef AF_INET6_IPV6
        } else if ( pNode->addr.sa.sa_family == AF_INET6 ) {
            pNode->addr.in6.sin6_port = htons ( port );
#endif
        }
        pNode = ( osiSockAddrNode46 * ) ellNext ( &pNode->node );
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
    char            addrautolistascii[32u];
    int             addrautolistIPversion;

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
    addrautolistIPversion = 4; /* IPv4 is used when EPICS_CA_AUTO_ADDR_LIST is not defined */
    pstr = envGetConfigParam ( &EPICS_CA_AUTO_ADDR_LIST,
                               sizeof (addrautolistascii), addrautolistascii );
    if ( pstr ) {
      if ( strstr ( pstr, "no" ) || strstr ( pstr, "NO" ) ) {
        addrautolistIPversion = 0;
      } else if ( strstr ( pstr, "yes" ) || strstr ( pstr, "YES" ) ) {
        addrautolistIPversion = 4;
      } else if ( !strcmp( pstr, "4" ) ) {
        addrautolistIPversion = 4;
      } else if ( !strcmp( pstr, "6" ) ) {
        addrautolistIPversion = 6;
      } else if ( !strcmp( pstr, "46" ) ) {
        addrautolistIPversion = 46;
      } else {
        errlogPrintf ( "EPICS_CA_AUTO_ADDR_LIST is invalid('%s'); IPv4 is used\n",
                       pstr);
      }
    }
#ifdef NETDEBUG
    epicsNetDebugLog("NET EPICS_CA_AUTO_ADDR_LIST='%s' addrautolistIPversion=%d\n",
		      pstr ? pstr : "",
		      addrautolistIPversion);

#endif

    /*
     * LOCK is for piiu->destAddr list
     * (lock outside because this is used by the server also)
     */
    if (addrautolistIPversion) {
        ELLLIST bcastList;
        osiSockAddr46 match46;
        ellInit ( &bcastList );
        memset(&match46, 0, sizeof(match46));
        match46.ia.sin_family = AF_INET; /* This is the default */
#ifdef AF_INET6_IPV6
        if (addrautolistIPversion == 46) {
          match46.ia.sin_family = AF_UNSPEC; /* Both v6 and v4 */
        } else if (addrautolistIPversion == 6) {
          match46.ia.sin_family = AF_INET6; /* v6 only */
        }
#endif
#ifdef NETDEBUG
        {
            char buf[64];
            sockAddrToDottedIP(&match46.sa, buf, sizeof(buf));
            epicsNetDebugLog("NET calling osiSockBroadcastMulticastAddresses46: sock=%d match46='%s'\n",
			      (int)sock, buf);
        }
#endif
        osiSockBroadcastMulticastAddresses46 ( &bcastList, sock, &match46 );
        forcePort ( &bcastList, port );
        removeDuplicateAddresses ( &tmpList, &bcastList, 1 );
        if ( ellCount ( &tmpList ) == 0 ) {
            osiSockAddrNode46 *pNewNode;
            pNewNode = (osiSockAddrNode46 *) calloc ( 1, sizeof (*pNewNode) );
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
    osiSockAddrNode46 *pNode;

    ::printf ( "Channel Access Address List\n" );
    pNode = (osiSockAddrNode46 *) ellFirst ( pList );
    while (pNode) {
        char buf[64];
        ipAddrToA ( &pNode->addr.ia, buf, sizeof ( buf ) );
        ::printf ( "%s\n", buf );
        pNode = (osiSockAddrNode46 *) ellNext ( &pNode->node );
    }
}
