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
#include "epicsTypes.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "errMdef.h"
#include "errSymTbl.h"
#include "ellLib.h"
#include "errlog.h"

#define NHASH 256

static epicsUInt16 errhash(long errNum);

typedef struct errnumnode {
    long               errNum;
    struct errnumnode *hashnode;
    const char        *message;
    long               pad;
} ERRNUMNODE;

typedef struct {
    ERRNUMNODE *table[NHASH * sizeof(ERRNUMNODE *)];
    epicsMutexId tableMutexId;
} errHashTable_t;

static errHashTable_t errHashTable;

extern ERRSYMTAB_ID errSymTbl;

/****************************************************************
 * ERRSYMBLD
 *
 * Populate EPICS error symbols
 *
 ***************************************************************/
int errSymBld(void)
{
    ERRSYMBOL *errArray = errSymTbl->symbols;
    int i;
    for (i = 0; i < errSymTbl->nsymbols; i++, errArray++) {
        if (errSymbolAdd(errArray->errNum, errArray->name)) {
            fprintf(stderr, "errSymBld: ERROR - errSymbolAdd() failed \n");
        }
    }

    return 0;
}

static void _initErrorHashTable(void *_unused)
{
    errHashTable.tableMutexId = epicsMutexMustCreate();
}

static void initErrorHashTable()
{
    static epicsThreadOnceId initErrSymOnceFlag = EPICS_THREAD_ONCE_INIT;
    epicsThreadOnce(&initErrSymOnceFlag, _initErrorHashTable, NULL);
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
    return ((modnum - (MIN_MODULE_NUM - 1)) * 20 + errnum) % NHASH;
}

/****************************************************************
 * ERRSYMBOLADD
 * adds symbols to the global error symbol hash table
 ***************************************************************/
int errSymbolAdd(long errNum, const char *name)
{
    ERRNUMNODE *pNextNode = NULL;
    ERRNUMNODE **phashnode = NULL;
    ERRNUMNODE *pNew = NULL;
    int modnum = (epicsUInt16) (errNum >> 16);
    epicsUInt16 hashInd = errhash(errNum);

    if (modnum < MIN_MODULE_NUM)
        return S_err_invCode;

    initErrorHashTable();

    epicsMutexLock(errHashTable.tableMutexId);
    phashnode = (ERRNUMNODE**)&errHashTable.table[hashInd];
    pNextNode = (ERRNUMNODE*) *phashnode;

    /* search for last node (NULL) of hashnode linked list */
    while (pNextNode) {
        if (pNextNode->errNum == errNum) {
            int notIdentical = strcmp(name, pNextNode->message);
            epicsMutexUnlock(errHashTable.tableMutexId);
            return  notIdentical ? S_err_codeExists : 0;
        }
        phashnode = &pNextNode->hashnode;
        pNextNode = *phashnode;
    }

    pNew = (ERRNUMNODE*) callocMustSucceed(
        1, sizeof(ERRNUMNODE), "errSymbolAdd");
    pNew->errNum = errNum;
    pNew->message = name;
    *phashnode = pNew;
    epicsMutexUnlock(errHashTable.tableMutexId);
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

    initErrorHashTable();

    modNum = (unsigned) status;
    modNum >>= 16;
    modNum &= 0xffff;
    if (modNum < MIN_MODULE_NUM) {
        return strerror ((int) status);
    }
    else {
        unsigned hashInd = errhash(status);
        const char *result = NULL;
        epicsMutexLock(errHashTable.tableMutexId);
        phashnode = (ERRNUMNODE**)&errHashTable.table[hashInd];
        pNextNode = *phashnode;
        while (pNextNode) {
            if (pNextNode->errNum==status){
                 result = pNextNode->message;
                 break;
            }
            phashnode = &pNextNode->hashnode;
            pNextNode = *phashnode;
        }
        epicsMutexUnlock(errHashTable.tableMutexId);
        return result;
    }
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

    initErrorHashTable();

    msgcount = 0;
    printf("errSymDump: number of hash slots = %d\n", NHASH);
    for (i = 0; i < NHASH; i++) {
        ERRNUMNODE **phashnode = &errHashTable.table[i];
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

    initErrorHashTable();

    message[0] = '\0';
    modnum = (epicsUInt16) (errNum >> 16);
    errnum = (epicsUInt16) (errNum & 0xffff);
    if (modnum < MIN_MODULE_NUM) {
        fprintf(stderr, "Usage:  errSymTestPrint(long errNum) \n");
        fprintf(stderr,
                "errSymTestPrint: module number < %d\n", MIN_MODULE_NUM);
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

    if (modnum < MIN_MODULE_NUM)
        return;

    initErrorHashTable();

    /* print range of error messages */
    for (errnum = begErrNum; errnum <= endErrNum; errnum++) {
        errNum = modnum << 16;
        errNum |= (errnum & 0xffff);
        errSymTestPrint(errNum);
    }
}
