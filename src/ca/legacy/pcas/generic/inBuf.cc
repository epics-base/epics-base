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

#include <stdexcept>
#include <new>

#include <stdio.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "inBuf.h"

//
// inBuf::inBuf()
//
inBuf::inBuf ( inBufClient & clientIn, clientBufMemoryManager & memMgrIn, 
              bufSizeT ioMinSizeIn ) :
    client ( clientIn ), memMgr ( memMgrIn ), pBuf ( 0 ), 
        bufSize ( 0u ), bytesInBuffer ( 0u ), nextReadIndex ( 0u ), 
        ioMinSize ( ioMinSizeIn ), ctxRecursCount ( 0u )
{
    if ( this->ioMinSize == 0 ) {
        this->ioMinSize = 1;
    }
    casBufferParm bufParm = this->memMgr.allocate ( this->ioMinSize );
    this->pBuf = bufParm.pBuf;
    this->bufSize = bufParm.bufSize;
}

//
// inBuf::~inBuf()
// (virtual destructor)
//
inBuf::~inBuf ()
{
    assert ( this->ctxRecursCount == 0 );
    this->memMgr.release ( this->pBuf, this->bufSize );
}

//
// inBuf::show()
//
void inBuf :: show (unsigned level) const
{
    if ( level > 1u ) {
        printf (
            "\tUnprocessed request bytes = %d\n",
            this->bytesPresent () );
    }
}

//
// inBuf::fill()
//
inBufClient::fillCondition inBuf::fill ( inBufClient::fillParameter parm )
{
	bufSizeT bytesOpen;
	bufSizeT bytesRecv;
	inBufClient::fillCondition stat;

	//
	// move back any prexisting data to the start of the buffer
	//
	if ( this->nextReadIndex > 0 ) {
		assert ( this->bytesInBuffer >= this->nextReadIndex );
		bufSizeT unprocessedBytes = 
		    this->bytesInBuffer - this->nextReadIndex;
		//
		// memmove() handles overlapping buffers
		//
		if (unprocessedBytes>0u) {
			memmove (this->pBuf, this->pBuf+this->nextReadIndex, 
				unprocessedBytes);
		}
		this->bytesInBuffer = unprocessedBytes;
		this->nextReadIndex = 0u;
	}

	//
	// noop if the buffer is full
	//
	bytesOpen = this->bufSize - this->bytesInBuffer;
	if ( bytesOpen < this->ioMinSize ) {
        return inBufClient::casFillNone;
	}

	stat = this->client.xRecv ( &this->pBuf[this->bytesInBuffer], 
			bytesOpen, parm, bytesRecv );
    if ( stat == inBufClient::casFillProgress ) {
	    assert (bytesRecv<=bytesOpen);
	    this->bytesInBuffer += bytesRecv;

	    if ( this->client.getDebugLevel() > 2u ) {
		    char buf[64];

		    this->client.hostName ( buf, sizeof ( buf ) );

		    fprintf ( stderr, "CAS Incoming: %u byte msg from %s\n",
			    bytesRecv, buf);
	    }
	}

	return stat;
}

//
// inBuf::pushCtx ()
//
const inBufCtx inBuf::pushCtx ( bufSizeT headerSize,
                               bufSizeT bodySize )
{
    if ( headerSize + bodySize > ( this->bytesInBuffer - this->nextReadIndex ) || 
        this->ctxRecursCount == UINT_MAX ) {
        return inBufCtx ();
    }
    else {
        inBufCtx result (*this);
        bufSizeT effectiveNextReadIndex = this->nextReadIndex + headerSize;
        this->pBuf = this->pBuf + effectiveNextReadIndex;
        this->bytesInBuffer -= effectiveNextReadIndex;
        this->nextReadIndex = 0;
        this->bytesInBuffer = bodySize;
        this->bufSize = this->bytesInBuffer;
        this->ctxRecursCount++;
        return result;
    }
}

//
// inBuf::popCtx ()
//
bufSizeT inBuf::popCtx ( const inBufCtx &ctx )
{
    if ( ctx.stat==inBufCtx::pushCtxSuccess ) {
        bufSizeT bytesRemoved = this->nextReadIndex;
        this->pBuf = ctx.pBuf;
        this->bufSize = ctx.bufSize;
        this->bytesInBuffer = ctx.bytesInBuffer;
        this->nextReadIndex = ctx.nextReadIndex;
        assert ( this->ctxRecursCount > 0 );
        this->ctxRecursCount--;
        return bytesRemoved;
    }
    else {
        return 0;
    }
}

void inBuf::expandBuffer (bufSizeT needed)
{
    if (needed > bufSize) {
        casBufferParm bufParm;
        try {
            bufParm = this->memMgr.allocate ( needed );
        } catch (std::bad_alloc& e) {
            // caller must check that buffer size has expended
            return;
        }

        bufSizeT unprocessedBytes = this->bytesPresent ();
        memcpy ( bufParm.pBuf, &this->pBuf[this->nextReadIndex], unprocessedBytes );
        this->bytesInBuffer = unprocessedBytes;
        this->nextReadIndex = 0u;
        this->memMgr.release ( this->pBuf, this->bufSize );
        this->pBuf = bufParm.pBuf;
        this->bufSize = bufParm.bufSize;
    }
}

bufSizeT inBuf::bufferSize() const
{
    return this->bufSize;
}


