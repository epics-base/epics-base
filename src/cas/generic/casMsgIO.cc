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
 *
 */




#include<server.h>

casMsgIO::casMsgIO()
{
	elapsedAtLastSend = this->elapsedAtLastRecv 
		= osiTime::getCurrent ();
}

casMsgIO::~casMsgIO()
{
}


//
// casMsgIO::show(unsigned level)
//
void casMsgIO::show(unsigned level)
{
        osiTime  elapsed;
        osiTime  current;
        double  send_delay;
        double  recv_delay;

        if(level>=1u){
		current = osiTime::getCurrent ();
		elapsed = current - this->elapsedAtLastSend;
		send_delay = elapsed;
		elapsed = current - this->elapsedAtLastRecv;
		recv_delay = elapsed;
                printf(
"\tSecs since last send %6.2f, Secs since last receive %6.2f\n",
        send_delay, recv_delay);
        }
}

xRecvStatus casMsgIO::xRecv(char *pBuf, bufSizeT nBytes, bufSizeT &nActualBytes)
{
	xRecvStatus stat;

	stat = this->osdRecv(pBuf, nBytes, nActualBytes);
	if (stat==xRecvOK) {
		this->elapsedAtLastRecv = osiTime::getCurrent();
	}
	return stat;
}

xSendStatus casMsgIO::xSend(char *pBuf, bufSizeT nBytes, bufSizeT &nActualBytes)
{
	xSendStatus stat;

	stat = this->osdSend(pBuf, nBytes, nActualBytes);
	if (stat == xSendOK) {
		this->elapsedAtLastSend = osiTime::getCurrent();
	}
	return stat;
}

void casMsgIO::sendBeacon(char & /*msg*/, bufSizeT /*length*/, 
		aitUint32 &/*m_avail*/)
{
	printf("virtual base sendBeacon() called?\n");
}

int casMsgIO::getFileDescriptor() const
{
	return -1;	// some os will not have file descriptors
}

void casMsgIO::setNonBlocking()
{
	printf("virtual base setNonBlocking() called?\n");
}

bufSizeT casMsgIO::incommingBytesPresent() const
{
	return 0u;
}

