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
 * errSymLib.c
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <limits.h>

#include "cantProceed.h"
#include "epicsAssert.h"
#include "epicsStdio.h"
#include "errMdef.h"
#include "errSymTbl.h"
#include "ellLib.h"
#include "errlog.h"

#define NHASH 256

static epicsUInt16 errhash(long errNum);

typedef struct errnumnode {
    ELLNODE            node;
    long               errNum;
    struct errnumnode *hashnode;
    const char        *message;
    long               pad;
} ERRNUMNODE;

static ELLLIST errnumlist = ELLLIST_INIT;
static ERRNUMNODE **hashtable;
static int initialized = 0;
extern ERRSYMTAB_ID errSymTbl;

/****************************************************************
 * ERRSYMBLD
 *
 * Create the normal ell LIST of sorted error messages nodes
 * Followed by linked hash lists - that link together those
 * ell nodes that have a common hash number.
 *
 ***************************************************************/
int errSymBld(void)
{
    ERRSYMBOL      *errArray = errSymTbl->symbols;
    ERRNUMNODE     *perrNumNode = NULL;
    ERRNUMNODE     *pNextNode = NULL;
    ERRNUMNODE    **phashnode = NULL;
    int             i;
    int             modnum;

    if (initialized)
        return(0);

    hashtable = (ERRNUMNODE**)callocMustSucceed
        (NHASH, sizeof(ERRNUMNODE*),"errSymBld");
    for (i = 0; i < errSymTbl->nsymbols; i++, errArray++) {
        modnum = errArray->errNum >> 16;
        if (modnum < 501) {
            fprintf(stderr, "errSymBld: ERROR - Module number in errSymTbl < 501 was Module=%lx Name=%s\n",
                errArray->errNum, errArray->name);
            continue;
        }
        if ((errSymbolAdd(errArray->errNum, errArray->name)) < 0) {
            fprintf(stderr, "errSymBld: ERROR - errSymbolAdd() failed \n");
            continue;
        }
    }
    perrNumNode = (ERRNUMNODE *) ellFirst(&errnumlist);
    while (perrNumNode) {
        /* hash each perrNumNode->errNum */
        epicsUInt16 hashInd = errhash(perrNumNode->errNum);

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
    initialized = 1;
    return(0);
}

/****************************************************************
 * HASH
 * returns the hash index of errNum
****************************************************************/
static epicsUInt16 errhash(long errNum)
{
    epicsUInt16 modnum;
    epicsUInt16 errnum;

    modnum = (unsigned short) (errNum >> 16);
    errnum = (unsigned short) (errNum & 0xffff);
    return (((modnum - 500) * 20) + errnum) % NHASH;
}

/****************************************************************
 * ERRSYMBOLADD
 * adds symbols to the master errnumlist as compiled from errSymTbl.c
 ***************************************************************/
int errSymbolAdd(long errNum, const char *name)
{
    ERRNUMNODE *pNew = (ERRNUMNODE*) callocMustSucceed(1,
        sizeof(ERRNUMNODE), "errSymbolAdd");

    pNew->errNum = errNum;
    pNew->message = name;
    ellAdd(&errnumlist, (ELLNODE*)pNew);
    return 0;
}

/****************************************************************
 * errRawCopy
 ***************************************************************/
static void errRawCopy(long statusToDecode, char *pBuf, size_t bufLength)
{
    epicsUInt16 modnum = (statusToDecode >> 16) & 0xffff;
    epicsUInt16 errnum = statusToDecode & 0xffff;

    assert(bufLength > 20);

    if (modnum == 0) {
        epicsSnprintf(pBuf, bufLength, "Error #%u", errnum);
    }
    else {
        epicsSnprintf(pBuf, bufLength, "Error (%u,%u)", modnum, errnum);
    }
}

static
const char* errSymLookupInternal(long status)
{
    unsigned modNum;
    ERRNUMNODE *pNextNode;
    ERRNUMNODE **phashnode = NULL;

    if (!status)
        return "Ok";

    if (!initialized)
        errSymBld();

    modNum = (unsigned) status;
    modNum >>= 16;
    modNum &= 0xffff;
    if (modNum <= 500) {
        const char * pStr = strerror ((int) status);
        if (pStr) {
            return pStr;
        }
    }
    else {
        unsigned hashInd = errhash(status);
        phashnode = (ERRNUMNODE**)&hashtable[hashInd];
        pNextNode = *phashnode;
        while (pNextNode) {
            if (pNextNode->errNum==status){
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
void errSymLookup(long status, char * pBuf, size_t bufLength)
{
    const char* msg = errSymLookupInternal(status);
    if(msg) {
        strncpy(pBuf, msg, bufLength-1);
        pBuf[bufLength-1] = '\0';
        return;
    }
    errRawCopy(status, pBuf, bufLength);
}

/****************************************************************
 * errSymDump
 ***************************************************************/
void errSymDump(void)
{
    int i;
    int msgcount = 0;

    if (!initialized) errSymBld();

    msgcount = 0;
    printf("errSymDump: number of hash slots = %d\n", NHASH);
    for (i = 0; i < NHASH; i++) {
        ERRNUMNODE **phashnode = &hashtable[i];
        ERRNUMNODE *pNextNode = *phashnode;
        int count = 0;

        while (pNextNode) {
            int modnum = pNextNode->errNum >> 16;
            int errnum = pNextNode->errNum & 0xffff;

            if (!count++) {
                printf("HASHNODE = %d\n", i);
            }
            printf("\tmod %d num %d \"%s\"\n",
                modnum , errnum , pNextNode->message);
            phashnode = &pNextNode->hashnode;
            pNextNode = *phashnode;
        }
        msgcount += count;
    }
    printf("\nerrSymDump: total number of error messages = %d\n", msgcount);
}


/****************************************************************
 * errSymTestPrint
 ***************************************************************/
void errSymTestPrint(long errNum)
{
    char        message[256];
    epicsUInt16 modnum;
    epicsUInt16 errnum;

    if (!initialized) errSymBld();

    message[0] = '\0';
    modnum = (epicsUInt16) (errNum >> 16);
    errnum = (epicsUInt16) (errNum & 0xffff);
    if (modnum < 501) {
        fprintf(stderr, "Usage:  errSymTestPrint(long errNum) \n");
        fprintf(stderr, "errSymTestPrint: module number < 501 \n");
        return;
    }
    errSymLookup(errNum, message, sizeof(message));
    if ( message[0] == '\0' ) return;
    printf("module %hu number %hu message=\"%s\"\n",
        modnum, errnum, message);
}

/****************************************************************
 * ERRSYMTEST
****************************************************************/
void errSymTest(epicsUInt16 modnum, epicsUInt16 begErrNum,
    epicsUInt16 endErrNum)
{
    long         errNum;
    epicsUInt16  errnum;

    if (!initialized) errSymBld();
    if (modnum < 501)
        return;

    /* print range of error messages */
    for (errnum = begErrNum; errnum <= endErrNum; errnum++) {
        errNum = modnum << 16;
        errNum |= (errnum & 0xffff);
        errSymTestPrint(errNum);
    }
}
