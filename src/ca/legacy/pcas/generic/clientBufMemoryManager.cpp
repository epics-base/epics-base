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

#define epicsExportSharedSymbols
#include "clientBufMemoryManager.h"

bufSizeT clientBufMemoryManager::maxSize () const
{
    return bufferFactory.largeBufferSize ();
}

casBufferParm clientBufMemoryManager::allocate ( bufSizeT newMinSize )
{
    casBufferParm parm;
    if ( newMinSize <= bufferFactory.smallBufferSize () ) {
        parm.pBuf = bufferFactory.newSmallBuffer ();
        parm.bufSize = bufferFactory.smallBufferSize ();
    }
    else if ( newMinSize <= bufferFactory.largeBufferSize () ) {
        parm.pBuf = bufferFactory.newLargeBuffer ();
        parm.bufSize = bufferFactory.largeBufferSize ();
    }
    else {
        parm.pBuf = static_cast < char * > ( ::operator new ( newMinSize ) );
        parm.bufSize = newMinSize;
    }
    return parm;
}

void clientBufMemoryManager::release ( char * pBuf, bufSizeT bufSize )
{
    if ( bufSize == bufferFactory.smallBufferSize () ) {
        bufferFactory.destroySmallBuffer ( pBuf );
    }
    else if ( bufSize == bufferFactory.largeBufferSize () ) {
        bufferFactory.destroyLargeBuffer ( pBuf );
    }
    else {
        ::operator delete ( pBuf );
    }
}

