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
 * Revision 1.5  1997/06/13 09:15:59  jhill
 * connect proto changes
 *
 * Revision 1.4  1997/04/10 19:34:13  jhill
 * API changes
 *
 * Revision 1.3  1996/11/02 00:54:19  jhill
 * many improvements
 *
 * Revision 1.2  1996/09/16 18:24:04  jhill
 * vxWorks port changes
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */

#include"server.h"

class casMsgIO {
public:
        casMsgIO();
        virtual ~casMsgIO();
 
        osiTime timeOfLastXmit() const;
        osiTime timeOfLAstRecv() const;
 
        //
        // show status of IO subsystem
        // (cant be const because a lock is taken)
        //
        void show (unsigned level) const;
 
        //
        // io independent send/recv
        //
        xSendStatus xSend (char *pBuf, bufSizeT nBytesAvailableToSend,
                        bufSizeT nBytesNeedToBeSent, bufSizeT &nBytesSent,
                        const caNetAddr &addr);
        xRecvStatus xRecv (char *pBuf, bufSizeT nBytesToRecv,
                        bufSizeT &nByesRecv, caNetAddr &addr);
 
        virtual bufSizeT incommingBytesPresent() const;
        virtual casIOState state() const=0;
        virtual void clientHostName (char *pBuf, unsigned bufSize) const =0;
        virtual int getFD() const;
        void setNonBlocking()
        {
                this->xSetNonBlocking();
                this->blockingStatus = xIsntBlocking;
        }
 
        //
        // The server's port number
        // (to be used for connection requests)
        //
        virtual unsigned serverPortNumber()=0;
private:
        //
        // private data members
        //
        osiTime lastSendTS;
        osiTime lastRecvTS;
        xBlockingStatus blockingStatus;
 
        virtual xSendStatus osdSend (const char *pBuf, bufSizeT nBytesReq,
                        bufSizeT &nBytesActual, const caNetAddr &addr) =0;
        virtual xRecvStatus osdRecv (char *pBuf, bufSizeT nBytesReq,
                        bufSizeT &nBytesActual, caNetAddr &addr) =0;
        virtual void osdShow (unsigned level) const = 0;
        virtual void xSetNonBlocking();
};


casMsgIO::casMsgIO()
{
	this->lastSendTS = this->lastRecvTS = osiTime::getCurrent ();
	this->blockingStatus = xIsBlocking;
}

casMsgIO::~casMsgIO()
{
}

//
// casMsgIO::show(unsigned level) const
//
void casMsgIO::show(unsigned level) const
{
	osiTime  current;
	double  send_delay;
	double  recv_delay;
	
	if (level>=1u) {
		current = osiTime::getCurrent ();
		send_delay = current - this->lastSendTS;
		recv_delay = current - this->lastRecvTS;
		printf(
			"\tSecs since last send %6.2f, Secs since last receive %6.2f\n",
			send_delay, recv_delay);
	}
}

//
// casMsgIO::xRecv()
//
xRecvStatus casMsgIO::xRecv(char *pBuf, bufSizeT nBytes, 
	bufSizeT &nActualBytes, caNetAddr &from)
{
	xRecvStatus stat;

	stat = this->osdRecv(pBuf, nBytes, nActualBytes, from);
	if (stat==xRecvOK) {
		this->lastRecvTS = osiTime::getCurrent();
	}
	return stat;
}

//
//  casMsgIO::xSend()
//
xSendStatus casMsgIO::xSend(char *pBuf, bufSizeT nBytesAvailableToSend, 
	bufSizeT nBytesNeedToBeSent, bufSizeT &nActualBytes, 
	const caNetAddr &to)
{
	xSendStatus stat;
	bufSizeT nActualBytesDelta;

	assert (nBytesAvailableToSend>=nBytesNeedToBeSent);

	nActualBytes = 0u;
	if (this->blockingStatus == xIsntBlocking) {
		stat = this->osdSend(pBuf, nBytesAvailableToSend, 
				nActualBytes, to);
		if (stat == xSendOK) {
			this->lastSendTS = osiTime::getCurrent();
		}
		return stat;
	}

	while (nBytesNeedToBeSent) {
		stat = this->osdSend(pBuf, nBytesAvailableToSend, 
				nActualBytesDelta, to);
		if (stat != xSendOK) {
			return stat;
		}

		this->lastSendTS = osiTime::getCurrent();
		nActualBytes += nActualBytesDelta;
		if (nBytesNeedToBeSent>nActualBytesDelta) {
			nBytesNeedToBeSent -= nActualBytesDelta;
		}
		else {
			break;
		}
	}
	return xSendOK;
}

int casMsgIO::getFD() const
{
	return -1;	// some os will not have file descriptors
}

void casMsgIO::xSetNonBlocking()
{
	printf("virtual base casMsgIO::xSetNonBlocking() called?\n");
}

bufSizeT casMsgIO::incommingBytesPresent() const
{
	return 0u;
}

