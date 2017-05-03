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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include <new>

#include <stdlib.h>

#include "epicsAssert.h"
#include "freeList.h"

#define epicsExportSharedSymbols
#include "clientBufMemoryManager.h"
#include "caProto.h"

clientBufMemoryManager::clientBufMemoryManager()
    :smallBufFreeList ( 0 )
{
    freeListInitPvt ( & this->smallBufFreeList, MAX_MSG_SIZE, 8 );
}

clientBufMemoryManager::~clientBufMemoryManager()
{
    freeListCleanup ( this->smallBufFreeList );
}

casBufferParm clientBufMemoryManager::allocate ( bufSizeT newMinSize )
{
    casBufferParm parm;
    if ( newMinSize <= MAX_MSG_SIZE ) {
        parm.pBuf = (char*)freeListMalloc(this->smallBufFreeList);
        parm.bufSize = MAX_MSG_SIZE;
    }
    else {
        // round size up to multiple of 4K
        newMinSize = ((newMinSize-1)|0xfff)+1;
        parm.pBuf = (char*)malloc(newMinSize);
        parm.bufSize = newMinSize;
    }
    if(!parm.pBuf)
        throw std::bad_alloc();
    return parm;
}

void clientBufMemoryManager::release ( char * pBuf, bufSizeT bufSize )
{
    assert(pBuf);
    if (bufSize <= MAX_MSG_SIZE) {
        freeListFree(this->smallBufFreeList, pBuf);
    }
    else {
        free(pBuf);
    }
}

