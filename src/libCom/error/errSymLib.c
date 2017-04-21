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
 * errSymLib.c
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 *
 ***************************************************************************
 * This must ultimately be replaced by a facility that allows remote
 * nodes access to the error messages. A message handling communication
 * task should be written that allows multiple remote nodes to request
 * notification of all error messages.
 * For now lets just print messages and last errno via logMsg or printf
 ***************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <limits.h>

#define epicsExportSharedSymbols
#include "cantProceed.h"
#include "epicsAssert.h"
#include "dbDefs.h"
#include "errMdef.h"
#include "errSymTbl.h"
#include "ellLib.h"
#include "errlog.h"


static unsigned short errhash(long errNum);

typedef struct errnumnode {
    ELLNODE             node;
    long                errNum;
    struct errnumnode *hashnode;
    char               *message;
    long                pad;
} ERRNUMNODE;
#define NHASH 256


static ELLLIST errnumlist = ELLLIST_INIT;
static ERRNUMNODE **hashtable;
static int initialized = FALSE;
extern ERRSYMTAB_ID errSymTbl;

/****************************************************************
 * ERRSYMBLD
 *
 * Create the normal ell LIST of sorted error messages nodes
 * Followed by linked hash lists - that link together those
 * ell nodes that have a common hash number.
 *
 ***************************************************************/
int epicsShareAPI errSymBld(void)
{
    ERRSYMBOL      *errArray = errSymTbl->symbols;
    ERRNUMNODE     *perrNumNode = NULL;
    ERRNUMNODE     *pNextNode = NULL;
    ERRNUMNODE    **phashnode = NULL;
    int             i;
    int             modnum;
    unsigned short  hashInd;

    if(initialized) return(0);
    hashtable = (ERRNUMNODE**)callocMustSucceed
        (NHASH, sizeof(ERRNUMNODE*),"errSymBld");
    for (i = 0; i < errSymTbl->nsymbols; i++, errArray++) {
	modnum = errArray->errNum >> 16;
	if (modnum < 501) {
	    fprintf(stderr, "errSymBld: ERROR - Module number in errSymTbl < 501 was Module=%lx Name=%s\n",
		errArray->errNum, errArray->name);
	    continue;
	}
	if ((errSymbolAdd(errArray->errNum, errArray->name))  <0 ) {
	    fprintf(stderr, "errSymBld: ERROR - errSymbolAdd() failed \n");
	    continue;
	}
    }
    perrNumNode = (ERRNUMNODE *) ellFirst(&errnumlist);
    while (perrNumNode) {
	/* hash each perrNumNode->errNum */
	hashInd = errhash(perrNumNode->errNum);
	phashnode = (ERRNUMNODE**)&hashtable[hashInd];
	pNextNode = (ERRNUMNODE*) *phashnode;
	/* search for last node (NULL) of hashnode linked list */
	while (pNextNode) {
	    phashnode = &pNextNode->hashnode;
	    pNextNode = *phashnode;
	}
	*phashnode = perrNumNode;
	perrNumNode = (ERRNUMNODE *) ellNext((ELLNODE *) perrNumNode);
    }
    initialized = TRUE;
    return(0);
}



/****************************************************************
 * HASH
 * returns the hash index of errNum
****************************************************************/
static unsigned short errhash(long errNum)
{
unsigned short modnum;
unsigned short errnum;

	modnum = (unsigned short) (errNum >> 16);
	errnum = (unsigned short) (errNum & 0xffff);
	return((unsigned short)(((modnum - 500) * 20) + errnum) % NHASH);
}

/****************************************************************
 * ERRSYMBOLADD
 * adds symbols to the master errnumlist as compiled from errSymTbl.c
 ***************************************************************/
int epicsShareAPI errSymbolAdd (long errNum,char *name)
{
    ERRNUMNODE     *pNew;

    pNew = (ERRNUMNODE*)callocMustSucceed(1,sizeof(ERRNUMNODE),"errSymbolAdd");
    pNew->errNum = errNum;
    pNew->message = name;
    ellAdd(&errnumlist,(ELLNODE*)pNew);
    return(0);
}

/****************************************************************
 * errRawCopy
 ***************************************************************/
static void errRawCopy ( long statusToDecode, char *pBuf, unsigned bufLength )
{
    unsigned modnum, errnum;
    unsigned nChar;
    int status;

    modnum = (unsigned) statusToDecode; 
    modnum >>= 16;
    modnum &= 0xffff;
    errnum = (unsigned) statusToDecode;
    errnum &= 0xffff;

    if ( bufLength ) {
        if ( modnum == 0 ) {
            if ( bufLength > 11 ) {
                status = sprintf ( pBuf, "err = %d", errnum );
            }
            else if ( bufLength > 5 ) {
                status = sprintf ( pBuf, "%d", errnum );
            }
            else {
                strncpy ( pBuf,"<err copy fail>", bufLength );
                pBuf[bufLength-1] = '\0';
                status = 0;
            }
        }
        else {
            if ( bufLength > 50 ) {
                status = sprintf ( pBuf, 
                    "status = (%d,%d) not in symbol table", modnum, errnum );
            }
            else if ( bufLength > 25 ) {
                status = sprintf ( pBuf, 
                    "status = (%d,%d)", modnum, errnum );
            }
            else if ( bufLength > 15 ) {
                status = sprintf ( pBuf, 
                    "(%d,%d)", modnum, errnum );
            }
            else {
                strncpy ( pBuf, 
                    "<err copy fail>", bufLength);
                pBuf[bufLength-1] = '\0';
                status = 0;
            }
        }
        assert (status >= 0 );
        nChar = (unsigned) status;
        assert ( nChar < bufLength );
    }
}

static
const char* errSymLookupInternal(long status)
{
    unsigned modNum;
    unsigned hashInd;
    ERRNUMNODE *pNextNode;
    ERRNUMNODE **phashnode = NULL;

    if(!initialized) errSymBld();

    modNum = (unsigned) status;
    modNum >>= 16;
    modNum &= 0xffff;
    if ( modNum <= 500 ) {
        const char * pStr = strerror ((int) status);
        if ( pStr ) {
            return pStr;
        }
    }
    else {
        hashInd = errhash(status);
        phashnode = (ERRNUMNODE**)&hashtable[hashInd];
        pNextNode = *phashnode;
        while(pNextNode) {
            if(pNextNode->errNum==status){
                return pNextNode->message;
            }
            phashnode = &pNextNode->hashnode;
            pNextNode = *phashnode;
        }
    }
    return NULL;
}

const char* errSymMsg(long status)
{
    const char* msg = errSymLookupInternal(status);
    return msg ? msg : "<Unknown code>";
}

/****************************************************************
 * errSymLookup
 ***************************************************************/
void epicsShareAPI errSymLookup (long status, char * pBuf, unsigned bufLength)
{
    const char* msg = errSymLookupInternal(status);
    if(msg) {
        strncpy(pBuf, msg, bufLength);
        pBuf[bufLength-1] = '\0';
        return;
    }
    errRawCopy(status, pBuf, bufLength);
}

/****************************************************************
 * errSymDump
 ***************************************************************/
void epicsShareAPI errSymDump(void)
{
ERRNUMNODE    **phashnode = NULL;
ERRNUMNODE     *pNextNode;
int i;
int modnum;
int errnum;
int msgcount;
int firstTime;

	if (!initialized) errSymBld();

	msgcount = 0;
	printf("errSymDump: number of hash slots=%d\n", NHASH);		
	for ( i=0; i < NHASH; i++) {
	    phashnode = &hashtable[i];
	    pNextNode = *phashnode;
	    firstTime=1;
		while (pNextNode) {
		    if (firstTime) {
		        printf("HASHNODE=%d\n", i);		
		        firstTime=0;
		    }
		    modnum = pNextNode->errNum >> 16;
		    errnum = pNextNode->errNum & 0xffff;
		    printf("\tmod %d num %d \"%s\"\n"
			, modnum , errnum , pNextNode->message);
		    msgcount++;
		    phashnode = &pNextNode->hashnode;
		    pNextNode = *phashnode;
		}
	}
	printf("\nerrSymDump: total number of error messages=%d\n", msgcount);		
}


/****************************************************************
 * errSymTestPrint
 ***************************************************************/
void epicsShareAPI errSymTestPrint(long errNum)
{
    char            message[256];
    unsigned short modnum;
    unsigned short errnum;

    if (!initialized) errSymBld();

    message[0] = '\0';
    modnum = (unsigned short) (errNum >> 16);
    errnum = (unsigned short) (errNum & 0xffff);
    if (modnum < 501) {
        fprintf(stderr, "Usage:  errSymTestPrint(long errNum) \n");
        fprintf(stderr, "errSymTestPrint: module number < 501 \n");
        return;
    }
    errSymLookup(errNum, message, sizeof(message));
    if ( message[0] == '\0' ) return;
    printf("module %hu number %hu message=\"%s\"\n",
		modnum, errnum, message);
    return;
}

/****************************************************************
 * ERRSYMTEST
****************************************************************/
void epicsShareAPI errSymTest(unsigned short modnum,
    unsigned short begErrNum, unsigned short endErrNum)
{
    long            errNum;
    unsigned short  errnum;

    if(!initialized) errSymBld();
    if (modnum < 501)
	return;

    /* print range of error messages */
    for (errnum = begErrNum; errnum <= endErrNum; errnum++) {
	errNum = modnum << 16;
	errNum |= (errnum & 0xffff);
	errSymTestPrint(errNum);
    }
}
