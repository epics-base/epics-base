/* 
 * $Id$
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

#include "envDefs.h"
#include "epicsAssert.h"
#include "locationException.h"

#define epicsExportSharedSymbols
#include "addrList.h"
#undef epicsExportSharedSymbols

#include "iocinf.h"

/*
 * getToken()
 */
static char *getToken ( const char **ppString, char *pBuf, unsigned bufSIze )
{
    const char *pToken;
    unsigned i;

    pToken = *ppString;
    while(isspace(*pToken)&&*pToken){
        pToken++;
    }

    for ( i=0u; i<bufSIze; i++ ) {
        if ( isspace (pToken[i]) || pToken[i]=='\0' ) {
            pBuf[i] = '\0';
            break;
        }
        pBuf[i] = pToken[i];
    }

    *ppString = &pToken[i];

    if(*pToken){
        return pBuf;
    }
    else{
        return NULL;
    }
}

/*
 * addAddrToChannelAccessAddressList ()
 */
extern "C" void epicsShareAPI addAddrToChannelAccessAddressList 
    ( ELLLIST *pList, const ENV_PARAM *pEnv, unsigned short port )
{
    osiSockAddrNode *pNewNode;
    const char *pStr;
    const char *pToken;
    struct sockaddr_in addr;
    char buf[32u]; /* large enough to hold an IP address */
    int status;

    pStr = envGetConfigParamPtr (pEnv);
    if (!pStr) {
        return;
    }

    while ( ( pToken = getToken (&pStr, buf, sizeof (buf) ) ) ) {
        status = aToIPAddr ( pToken, port, &addr );
        if (status<0) {
            fprintf ( stderr, "%s: Parsing '%s'\n", __FILE__, pEnv->name);
            fprintf ( stderr, "\tBad internet address or host name: '%s'\n", pToken);
            continue;
        }

        pNewNode = (osiSockAddrNode *) calloc (1, sizeof(*pNewNode));
        if (pNewNode==NULL) {
            fprintf ( stderr, "addAddrToChannelAccessAddressList(): no memory available for configuration\n");
            return;
        }

        pNewNode->addr.ia = addr;
        memset ( &pNewNode->netMask, '\0', sizeof ( pNewNode->netMask ) );

		/*
		 * LOCK applied externally
		 */
        ellAdd (pList, &pNewNode->node);
    }

    return;
}

/*
 * removeDuplicatesAddresses ()
 */
extern "C" void epicsShareAPI removeDuplicatesAddresses 
    ( ELLLIST *pDestList, ELLLIST *pSrcList )
{
    osiSockAddrNode *pNode;

    while ( (pNode  = (osiSockAddrNode *) ellGet ( pSrcList ) ) ) {
        osiSockAddrNode *pTmpNode;

        if ( pNode->addr.sa.sa_family == AF_INET ) {

            pTmpNode = (osiSockAddrNode *) ellFirst (pDestList);
            while ( pTmpNode ) {
                if (pTmpNode->addr.sa.sa_family == AF_INET) {
                    unsigned netMask = pNode->netMask.ia.sin_addr.s_addr | 
                        pTmpNode->netMask.ia.sin_addr.s_addr;
                    unsigned exorAddr = pNode->addr.ia.sin_addr.s_addr ^ pTmpNode->addr.ia.sin_addr.s_addr;
                    exorAddr &= netMask;
                    if ( exorAddr == 0u && pNode->addr.ia.sin_port == pTmpNode->addr.ia.sin_port) {
                        char buf[64];
                        ipAddrToDottedIP ( &pNode->addr.ia, buf, sizeof (buf) );
                        fprintf ( stderr, "Warning: Duplicate EPICS CA Address list entry \"%s\" discarded\n", buf );
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
            pNode->addr.ia.sin_port = htons (port);
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
        osiSockAddr addr;
        addr.ia.sin_family = AF_UNSPEC;
        osiSockDiscoverBroadcastAddresses ( &tmpList, sock, &addr );
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
                memset ( &pNewNode->netMask, '\0', sizeof ( pNewNode->netMask ) );
                ellAdd ( &tmpList, &pNewNode->node );
            }
            else {
                errlogPrintf ( "configureChannelAccessAddressList(): no memory available for configuration\n" );
            }
        }
        else {
            forcePort ( &tmpList, port );
        }
    }
    addAddrToChannelAccessAddressList ( &tmpList, &EPICS_CA_ADDR_LIST, port );

    removeDuplicatesAddresses ( pList, &tmpList );
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
