/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 */

#include "server.h"
#include "inBufIL.h" // inBuf in line func

//
// inBuf::inBuf()
//
inBuf::inBuf (bufSizeT bufSizeIn, bufSizeT ioMinSizeIn) :
    bufSize (bufSizeIn), 
    bytesInBuffer (0u), 
    nextReadIndex (0u),
    ioMinSize (ioMinSizeIn), 
    ctxRecursCount (0u)
{
    if (this->ioMinSize==0) {
        this->ioMinSize = 1;
    }

    assert (this->bufSize>this->ioMinSize);

    this->pBuf = new char [this->bufSize];
    if (!this->pBuf) {
        this->bufSize = 0u;
        throw S_cas_noMemory;
    }
}

//
// inBuf::~inBuf()
// (virtual destructor)
//
inBuf::~inBuf ()
{
    assert (this->ctxRecursCount==0);
	delete [] this->pBuf;
}

//
// inBuf::show()
//
void inBuf::show (unsigned level) const
{
    if (level>1u) {
        printf(
            "\tUnprocessed request bytes = %d\n",
            this->bytesAvailable());
    }
}

//
// inBuf::fill()
//
inBuf::fillCondition inBuf::fill (fillParameter parm)
{
	bufSizeT bytesOpen;
	bufSizeT bytesRecv;
	inBuf::fillCondition stat;

	//
	// move back any prexisting data to the start of the buffer
	//
	if ( this->nextReadIndex > 0 ) {
		bufSizeT unprocessedBytes;
		unprocessedBytes = this->bytesInBuffer - this->nextReadIndex;
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
	if (bytesOpen < this->ioMinSize) {
		return casFillNone;
	}

	stat = this->xRecv (&this->pBuf[this->bytesInBuffer], 
			bytesOpen, parm, bytesRecv);
	if (stat == casFillProgress) {
	    assert (bytesRecv<=bytesOpen);
	    this->bytesInBuffer += bytesRecv;

	    if (this->getDebugLevel()>2u) {
		    char buf[64];

		    this->clientHostName(buf, sizeof(buf));

		    printf ("CAS: incomming %u byte msg from %s\n",
			    bytesRecv, buf);
	    }
	}

	return stat;
}

//
// inBuf::pushCtx ()
//
const inBufCtx inBuf::pushCtx (bufSizeT headerSize, bufSizeT bodySize)
{
    if (headerSize+bodySize>(this->bytesInBuffer - this->nextReadIndex) || 
        this->ctxRecursCount==UINT_MAX) {
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
bufSizeT inBuf::popCtx (const inBufCtx &ctx)
{
    if (ctx.stat==inBufCtx::pushCtxSuccess) {
        bufSizeT bytesRemoved = this->nextReadIndex;
        this->pBuf = ctx.pBuf;
        this->bufSize = ctx.bufSize;
        this->bytesInBuffer = ctx.bytesInBuffer;
        this->nextReadIndex = ctx.nextReadIndex;
        assert (this->ctxRecursCount>0);
        this->ctxRecursCount--;
        this->mutex.unlock();
        return bytesRemoved;
    }
    else {
        return 0;
    }
}



