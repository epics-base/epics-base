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

#include "server.h"
#include "inBufIL.h" // inBuf in line func

//
// inBuf::~inBuf()
// (virtual destructor)
//
inBuf::~inBuf()
{
	if (this->pBuf) {
		delete [] this->pBuf;
	}
}

//
// inBuf::show()
//
void inBuf::show(unsigned level) const
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
casFillCondition inBuf::fill()
{
	unsigned bytesReq;
	unsigned bytesRecv;
	xRecvStatus stat;

	if (!this->pBuf) {
		return casFillDisconnect;
	}

	//
	// move back any prexisting data to the start of the buffer
	//
	if (this->nextReadIndex>0u) {
		unsigned unprocessedBytes;
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
	bytesReq = this->bufSize - this->bytesInBuffer;
	if (bytesReq <= 0u) {
		return casFillFull;
	}

	stat = this->xRecv(&this->pBuf[this->bytesInBuffer], 
			bytesReq, bytesRecv);
	if (stat != xRecvOK) {
		return casFillDisconnect;
	}
	if (bytesRecv==0u) {
		return casFillNone;
	}
	assert (bytesRecv<=bytesReq);
	this->bytesInBuffer += bytesRecv;

	if (this->getDebugLevel()>2u) {
		char buf[64];

		this->clientHostName(buf, sizeof(buf));

		printf ("CAS: incomming %u byte msg from %s\n",
			bytesRecv, buf);
	}

	if (this->bufSize==this->bytesInBuffer) {
		return casFillFull;
	}
	else {
		return casFillPartial;
	}
}

