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
 * History
 * $Log$
 * Revision 1.3  1996/09/04 20:27:01  jhill
 * doccasdef.h
 *
 * Revision 1.2  1996/08/13 22:53:59  jhill
 * fixed little endian problem
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */



#include<server.h>

//
// outBuf::outBuf()
//
outBuf::outBuf(casMsgIO &virtualCircuit, osiMutex &mutexIn) : 
	io(virtualCircuit),
	mutex(mutexIn),
	pBuf(NULL),
	bufSize(io.optimumBufferSize()),
	stack(0u)
{
	assert(&io);
}

//
// outBuf::init()
//
caStatus outBuf::init() 
{
	this->pBuf = new char [this->bufSize];
	if (!this->pBuf) {
		return S_cas_noMemory;
	}
	return S_cas_success;
}

//
// outBuf::~outBuf()
//
outBuf::~outBuf()
{
	if (this->pBuf) {
		delete [] this->pBuf;
	}
}


//
//	outBuf::allocMsg() 
//
//	allocates space in the outgoing message buffer
//
//	(if space is avilable this leaves the send lock applied)
//			
caStatus outBuf::allocMsg (
bufSizeT	extsize,	// extension size 
caHdr		**ppMsg
)
{
	bufSizeT msgsize;
	bufSizeT stackNeeded;
	
	extsize = CA_MESSAGE_ALIGN(extsize);

	msgsize = extsize + sizeof(caHdr);
	if (msgsize>this->bufSize || this->pBuf==NULL) {
		return S_cas_hugeRequest;
	}

	stackNeeded = this->bufSize - msgsize;

	this->mutex.osiLock();

	if (this->stack > stackNeeded) {
		
		/*
		 * Try to flush the output queue
		 */
		this->flush(casFlushSpecified, msgsize);

		/*
		 * If this failed then the fd is nonblocking 
		 * and we will let select() take care of it
		 */
		if (this->stack > stackNeeded) {
			this->mutex.osiUnlock();
			this->sendBlockSignal();
			return S_cas_sendBlocked;
		}
	}

	/*
	 * it fits so commitMsg() will move the stack pointer forward
	 */
	*ppMsg = (caHdr *) &this->pBuf[this->stack];

	return S_cas_success;
}



/*
 * outBuf::commitMsg()
 */
void outBuf::commitMsg ()
{
	caHdr 		*mp;
	bufSizeT	extSize;
	bufSizeT	diff;

	mp = (caHdr *) &this->pBuf[this->stack];

	extSize = CA_MESSAGE_ALIGN(mp->m_postsize);

	//
	// Guarantee that all portions of outgoing messages 
	// (including alignment pad) are initialized 
	//
	diff = extSize - mp->m_postsize;
	if (diff>0u) {
		char 		*pExt = (char *) (mp+1);
		memset(pExt + mp->m_postsize, '\0', diff);
	}

  	mp->m_postsize = extSize;

  	if (this->getDebugLevel()) {
		ca_printf (
	"CAS Response => cmd=%d id=%x typ=%d cnt=%d psz=%d avail=%x\n",
			mp->m_cmmd,
			mp->m_cid,
			mp->m_type,
			mp->m_count,
			mp->m_postsize,
			mp->m_available);
	}

	/*
	 * convert to network byte order
	 * (data following header is handled elsewhere)
	 */
	mp->m_cmmd = htons (mp->m_cmmd);
	mp->m_postsize = htons (mp->m_postsize);
	mp->m_type = htons (mp->m_type);
	mp->m_count = htons (mp->m_count);
	mp->m_cid = htonl (mp->m_cid);
	mp->m_available = htonl (mp->m_available);

    	this->stack += sizeof(caHdr) + extSize;
	assert (this->stack <= this->bufSize);

	this->mutex.osiUnlock();
}

//
// outBuf::flush()
//
casFlushCondition outBuf::flush(casFlushRequest req,
				bufSizeT spaceRequired)
{
	bufSizeT 		nBytes;
	bufSizeT 		stackNeeded;
	bufSizeT 		nBytesNeeded;
	xSendStatus		stat;
	casFlushCondition	cond;

	if (!this->pBuf) {
		return casFlushDisconnect;
	}

	this->mutex.osiLock();

	if (req==casFlushAll) {
		nBytesNeeded = this->stack;
	}
	else {
		stackNeeded = this->bufSize - spaceRequired;
		if (this->stack>stackNeeded) {
			nBytesNeeded = this->stack - stackNeeded;
		}
		else {
			nBytesNeeded = 0u;
		}
	}

	stat = this->io.xSend(this->pBuf, this->stack, 
			nBytesNeeded, nBytes);
	if (nBytes) {
		bufSizeT len;

		if (nBytes >= this->stack) {
			this->stack=0u;	
			cond = casFlushCompleted;
		}
		else {
			len = this->stack-nBytes;
			//
			// memmove() is ok with overlapping buffers
			//
			memmove (this->pBuf, &this->pBuf[nBytes], len);
			this->stack = len;
			cond = casFlushPartial;
		}
	}
	else {
		cond = casFlushNone;
	}

	if (stat!=xSendOK) {
		cond = casFlushDisconnect;
	}
	this->mutex.osiUnlock();
	return cond;

}

//
// outBuf::show(unsigned level)
//
void outBuf::show(unsigned level)
{
	if (level>1u) {
                printf(
			"\tUndelivered response bytes =%d\n",
                        this->bytesPresent());
	}
}

