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
 *
 */

#include "server.h"
#include "outBufIL.h" // outBuf in line func

//
// outBuf::outBuf()
//
outBuf::outBuf (unsigned bufSizeIn) : 
	bufSize (bufSizeIn), stack (0u), ctxRecursCount (0u)
{
	this->pBuf = new char [this->bufSize];
	if (!this->pBuf) {
		throw S_cas_noMemory;
	}
	memset (this->pBuf, '\0', this->bufSize);
}

//
// outBuf::~outBuf()
//
outBuf::~outBuf()
{
    assert (this->ctxRecursCount==0);
	delete [] this->pBuf;
}

//
// outBuf::allocRawMsg ()
//
caStatus outBuf::allocRawMsg (bufSizeT msgsize, void **ppMsg)
{
	bufSizeT stackNeeded;
	
	msgsize = CA_MESSAGE_ALIGN (msgsize);

	this->mutex.lock ();

	if (msgsize>this->bufSize) {
	    this->mutex.unlock ();
		return S_cas_hugeRequest;
	}

	stackNeeded = this->bufSize - msgsize;

	if (this->stack > stackNeeded) {
		
		//
		// Try to flush the output queue
		//
		this->flush (this->stack-stackNeeded);

		//
		// If this failed then the fd is nonblocking 
		// and we will let select() take care of it
		//
		if (this->stack > stackNeeded) {
			this->mutex.unlock();
			this->sendBlockSignal();
			return S_cas_sendBlocked;
		}
	}

	//
	// it fits so commitMsg() will move the stack pointer forward
	//
	*ppMsg = (void *) &this->pBuf[this->stack];

	return S_cas_success;
}

//
// outBuf::commitMsg ()
//
void outBuf::commitMsg ()
{
	caHdr 		*mp;
    ca_uint16_t diff;
	ca_uint16_t	extSize;

	mp = (caHdr *) &this->pBuf[this->stack];

	extSize = static_cast <ca_uint16_t> ( CA_MESSAGE_ALIGN (mp->m_postsize) );

	//
	// Guarantee that all portions of outgoing messages 
	// (including alignment pad) are initialized 
	//
	diff = extSize - mp->m_postsize;
	if (diff>0u) {
		memset ( reinterpret_cast<char *>(mp+1) + mp->m_postsize, '\0', diff );
	}

  	if (this->getDebugLevel()) {
		ca_printf (
"CAS Response => cmd=%d id=%x typ=%d cnt=%d psz=%d avail=%x outBuf ptr=%lx\n",
			mp->m_cmmd, mp->m_cid, mp->m_dataType, mp->m_count, mp->m_postsize,
			mp->m_available, (long)mp);
	}

	//
	// convert to network byte order
	// (data following header is handled elsewhere)
	//
	mp->m_cmmd = htons (mp->m_cmmd);
	mp->m_postsize = htons (extSize);
	mp->m_dataType = htons (mp->m_dataType);
	mp->m_count = htons (mp->m_count);
	mp->m_cid = htonl (mp->m_cid);
	mp->m_available = htonl (mp->m_available);

    commitRawMsg (extSize + sizeof(*mp));
}

//
// outBuf::flush ()
//
outBuf::flushCondition outBuf::flush (bufSizeT spaceRequired)
{
	bufSizeT 		nBytes;
    bufSizeT        nBytesRequired;
	flushCondition	cond;

	this->mutex.lock();

    if (this->ctxRecursCount>0) {
        this->mutex.unlock();
        return flushNone;
    }

	if (spaceRequired>this->bufSize) {
		nBytesRequired = this->stack;
	}
	else {
        bufSizeT stackPermitted;

		stackPermitted = this->bufSize - spaceRequired;
		if (this->stack>stackPermitted) {
			nBytesRequired = this->stack - stackPermitted;
		}
		else {
            nBytesRequired = 0u;
		}
	}

	cond = this->xSend(this->pBuf, this->stack, 
			nBytesRequired, nBytes);
	if (cond==flushProgress) {
		bufSizeT len;

		if (nBytes >= this->stack) {
			this->stack = 0u;	
		}
		else {
			len = this->stack-nBytes;
			//
			// memmove() is ok with overlapping buffers
			//
			memmove (this->pBuf, &this->pBuf[nBytes], len);
			this->stack = len;
		}

		if (this->getDebugLevel()>2u) {
		    char buf[64];
			this->clientHostName (buf, sizeof(buf));
			ca_printf ("CAS: Sent a %d byte reply to %s\n",
				nBytes, buf);
		}
    }
	this->mutex.unlock();
	return cond;
}

//
// outBuf::pushCtx ()
//
const outBufCtx outBuf::pushCtx (bufSizeT headerSize, bufSizeT maxBodySize, void *&pHeader)
{
    bufSizeT totalSize = headerSize + maxBodySize;
    caStatus status;

    status = this->allocRawMsg (totalSize, &pHeader);
    if (status!=S_cas_success) {
        return outBufCtx ();
    }
    else if (this->ctxRecursCount==UINT_MAX) {
        this->mutex.unlock();
        return outBufCtx ();
    }
    else {
        outBufCtx result (*this);
        this->pBuf = this->pBuf + this->stack + headerSize;
        this->stack = 0;
        this->bufSize = maxBodySize;
        this->ctxRecursCount++;
        return result;
    }
}

//
// outBuf::popCtx ()
//
bufSizeT outBuf::popCtx (const outBufCtx &ctx)
{
    if (ctx.stat==outBufCtx::pushCtxSuccess) {
        bufSizeT bytesAdded = this->stack;
        this->pBuf = ctx.pBuf;
        this->bufSize = ctx.bufSize;
        this->stack = ctx.stack;
        assert (this->ctxRecursCount>0u);
        this->ctxRecursCount--;
        return bytesAdded;
    }
    else {
        return 0;
    }
}

//
// outBuf::show (unsigned level)
//
void outBuf::show (unsigned level) const
{
    if (level>1u) {
        printf("\tUndelivered response bytes = %d\n", this->bytesPresent());
    }
}

