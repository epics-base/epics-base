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

#include "envDefs.h"
#include "freeList.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#include "clientBufMemoryManager.h"
#include "caProto.h"

casBufferFactory::casBufferFactory () :
    smallBufFreeList ( 0 ), largeBufFreeList ( 0 ), largeBufferSizePriv ( 0u )
{
    long maxBytesAsALong;
    long status =  envGetLongConfigParam ( & EPICS_CA_MAX_ARRAY_BYTES, & maxBytesAsALong );
    if ( status || maxBytesAsALong < 0 ) {
        errlogPrintf ( "cas: EPICS_CA_MAX_ARRAY_BYTES was not a positive integer\n" );
        this->largeBufferSizePriv = MAX_TCP;
    }
    else {
        /* allow room for the protocol header so that they get the array size they requested */
        static const unsigned headerSize = sizeof ( caHdr ) + 2 * sizeof ( ca_uint32_t );
        ca_uint32_t maxBytes = ( unsigned ) maxBytesAsALong;
        if ( maxBytes < 0xffffffff - headerSize ) {
            maxBytes += headerSize;
        }
        else {
            maxBytes = 0xffffffff;
        }
        if ( maxBytes < MAX_TCP ) {
            errlogPrintf ( "cas: EPICS_CA_MAX_ARRAY_BYTES was rounded up to %u\n", MAX_TCP );
            this->largeBufferSizePriv = MAX_TCP;
        }
        else {
            this->largeBufferSizePriv = maxBytes;
        }
    }

    freeListInitPvt ( & this->smallBufFreeList, MAX_MSG_SIZE, 8 );
    freeListInitPvt ( & this->largeBufFreeList, this->largeBufferSizePriv, 1 );
}

casBufferFactory::~casBufferFactory ()
{
    freeListCleanup ( this->smallBufFreeList );
    freeListCleanup ( this->largeBufFreeList );
}

unsigned casBufferFactory::smallBufferSize () const
{
    return MAX_MSG_SIZE;
}

char * casBufferFactory::newSmallBuffer ()
{
    void * pBuf = freeListCalloc ( this->smallBufFreeList );
    if ( ! pBuf ) {
        throw std::bad_alloc();
    }
    return static_cast < char * > ( pBuf );
}

void casBufferFactory::destroySmallBuffer ( char * pBuf )
{
    if ( pBuf ) {
        freeListFree ( this->smallBufFreeList, pBuf );
    }
}

unsigned casBufferFactory::largeBufferSize () const
{
    return this->largeBufferSizePriv;
}

char * casBufferFactory::newLargeBuffer ()
{
    void * pBuf = freeListCalloc ( this->largeBufFreeList );
    if ( ! pBuf ) {
        throw std::bad_alloc();
    }
    return static_cast < char * > ( pBuf );
}

void casBufferFactory::destroyLargeBuffer ( char * pBuf )
{
    if ( pBuf ) {
        freeListFree ( this->largeBufFreeList, pBuf );
    }
}
