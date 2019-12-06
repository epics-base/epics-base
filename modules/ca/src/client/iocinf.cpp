/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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

#include "epicsTypes.h"
#include "envDefs.h"
#include "epicsAssert.h"
#include "epicsStdioRedirect.h"
#include "errlog.h"
#include "osiWireFormat.h"

#define epicsExportSharedSymbols
#include "addrList.h"
#undef epicsExportSharedSymbols

#include "iocinf.h"

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

/* Examples for valid network specifications:
 * CIDR Notation:      193.245.13.0/24
 * ADDR:MASK Notation: 193.245.13.0:255.255.255.0
 * single IP address:  193.245.13.14
 * sets two uint32 integers in network byte order 
 * returns: 1 on success, 0 on error 
 * */
static int scan_netspec(const char *st, epicsUInt32 *addr, epicsUInt32 *mask)
  {
    int a[4]= { 0, 0, 0, 0};
    int b[4]= { 0, 0, 0, 0};
    int i,m=32;
    int rc;

    /* addr:mask format: */
    rc= sscanf(st, "%d.%d.%d.%d:%d.%d.%d.%d", a, a+1, a+2, a+3,
                                              b, b+1, b+2, b+3);
    if (rc!=8)
      {
        /* CIDR format: */
        rc= sscanf(st, "%d.%d.%d.%d/%d", a, a+1, a+2, a+3, &m);
        if (rc!=5)
          {
            /* single IP format: */
            rc= sscanf(st, "%d.%d.%d.%d", a, a+1, a+2, a+3);
            if (rc!=4)
              return 0;
          }
      }
    /* range checks: */
    for(i=0; i<4; i++)
      { 
        if ((a[i]<0) || (a[i]>255))
          return 0;
        if ((b[i]<0) || (b[i]>255))
          return 0;
      }
    if ((m<=0) || (m>32))
      return 0;

    *addr= htonl((a[0]<<24) | (a[1]<<16) | (a[2]<<8) | (a[3]));
    if (rc==8) /* netmask was given explicitly */
      *mask= htonl((b[0]<<24) | (b[1]<<16) | (b[2]<<8) | (b[3]));
    else
      {
        if (rc==4) /* only a single ip was provided */
          m= 32;
        *mask= htonl( 0xFFFFFFFF & (0xFFFFFFFF << (32-m)));
      }
    return 1;
  }

extern "C" int epicsShareAPI networkList
    ( ELLLIST *pList, const ENV_PARAM *pEnv )
{
    char buf[40]; /* large enough to hold a CIDR notation */
    osiSockNetNode *pNewNode;
    int ret = -1;
    epicsUInt32 addr, mask;
    const char *pStr;
    const char *pToken;

    pStr = envGetConfigParamPtr (pEnv);
    if (!pStr) {
        return ret;
    }
    while ( ( pToken = getToken (&pStr, buf, sizeof (buf) ) ) ) {
        if (!scan_netspec( pToken, &addr, &mask )) {
            fprintf ( stderr, "%s: Parsing '%s'\n", __FILE__, pEnv->name);
            fprintf ( stderr, "\tBad notation for network: '%s'\n", pToken);
            continue;
        }
        pNewNode = (osiSockNetNode *) calloc (1, sizeof(*pNewNode));
        if (pNewNode==NULL) {
            fprintf ( stderr, "networkList(): no memory available for configuration\n");
            break;
        }
        pNewNode->net.addr= addr;
        pNewNode->net.mask= mask;
		/*
		 * LOCK applied externally
		 */
        ellAdd (pList, &pNewNode->node);
        ret = 0; /* success if anything is added to the list */
    }
    return ret;
}

/*
 * addAddrToChannelAccessAddressList ()
 */
extern "C" int epicsShareAPI addAddrToChannelAccessAddressList
    ( ELLLIST *pList, const ENV_PARAM *pEnv, 
    unsigned short port, int ignoreNonDefaultPort )
{
    osiSockAddrNode *pNewNode;
    const char *pStr;
    const char *pToken;
    struct sockaddr_in addr;
    char buf[32u]; /* large enough to hold an IP address */
    int status, ret = -1;

    pStr = envGetConfigParamPtr (pEnv);
    if (!pStr) {
        return ret;
    }

    while ( ( pToken = getToken (&pStr, buf, sizeof (buf) ) ) ) {
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

    return ret;
}

/*
 * removeDuplicateAddresses ()
 */
extern "C" void epicsShareAPI removeDuplicateAddresses 
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
extern "C" void epicsShareAPI configureChannelAccessAddressList 
        ( ELLLIST *pList, SOCKET sock, unsigned short port )
{
    ELLLIST         tmpList;
    char            *pstr;
    char            yesno[32u];
    int             yes;

    /*
     * dont load the list twice
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
extern "C" void epicsShareAPI printChannelAccessAddressList ( const ELLLIST *pList )
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
