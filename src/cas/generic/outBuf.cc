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
	mutex(mutexIn)
{
	assert(&io);

	this->stack = 0u;
	this->bufSize = 0u;
	this->pBuf = NULL;
}

//
// outBuf::init()
//
caStatus outBuf::init() 
{
	this->bufSize = io.optimumBufferSize();
	this->pBuf = new char [this->bufSize];
	if (!this->pBuf) {
		this->bufSize = 0u;
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
	
	extsize = CA_MESSAGE_ALIGN(extsize);

	msgsize = extsize + sizeof(caHdr);
	if (msgsize>this->bufSize) {
		return S_cas_hugeRequest;
	}


	this->mutex.lock();

	if (this->stack + msgsize > this->bufSize) {
		
		/*
		 * Try to flush the output queue
		 */
		this->flush();

		/*
		 * If this failed then the fd is nonblocking 
		 * and we will let select() take care of it
		 */
		if (this->stack + msgsize > this->bufSize) {
			this->mutex.unlock();
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
	mp->m_count = htons (mp->m_count);
	mp->m_cid = htonl (mp->m_cid);
	mp->m_available = htonl (mp->m_available);

    	this->stack += sizeof(caHdr) + extSize;
	assert (this->stack <= this->bufSize);

	this->mutex.unlock();
}

//
// outBuf::flush()
//
casFlushCondition outBuf::flush()
{
	bufSizeT 	nBytes;
	xSendStatus	stat;

	if (this->stack<=0u) {
		return casFlushCompleted;
	}

	stat = this->io.xSend(this->pBuf, this->stack, nBytes);
	if (stat!=xSendOK) {
		return casFlushDisconnect;
	}
	else if (nBytes >= this->stack) {
		this->stack=0u;	
		return casFlushCompleted;
	}
	else if (nBytes) {
		bufSizeT len;

		len = this->stack-nBytes;
		//
		// memmove() is ok with overlapping buffers
		//
		memmove (this->pBuf, &this->pBuf[nBytes], len);
		this->stack = len;
		return casFlushPartial;
	}
	else {
		return casFlushNone;
	}
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

