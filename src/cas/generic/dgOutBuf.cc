/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "server.h"
#include "outBufIL.h"

//
// this needs to be here (and not in dgInBufIL.h) if we
// are to avoid undefined symbols under gcc 2.7.x when
// -O is not used
//
// dgOutBuf::dgOutBuf ()
//
dgOutBuf::dgOutBuf (osiMutex &mutexIn, unsigned bufSizeIn) :
                outBuf(mutexIn, bufSizeIn)
{
}

//
// dgOutBuf::~dgOutBuf()
// (force virtual destructor)
//
dgOutBuf::~dgOutBuf()
{
}

//
// dgOutBuf::xSend()
//
xSendStatus dgOutBuf::xSend (char *pBufIn,
        bufSizeT nBytesAvailableToSend, bufSizeT nBytesNeedToBeSent,
        bufSizeT &nBytesSent)
{
        assert(nBytesAvailableToSend>=nBytesNeedToBeSent);
        return xDGSend(pBufIn, nBytesAvailableToSend, nBytesSent, this->to);
}

//
// this needs to be here (and not in dgInBufIL.h) if we
// are to avoid undefined symbols under gcc 2.7.x (without -O)
//
// dgOutBuf::clear()
//
void dgOutBuf::clear()
{
        this->to.clear();
        this->outBuf::clear();
}
 
//
// this needs to be here (and not in dgInBufIL.h) if we
// are to avoid undefined symbols under gcc 2.7.x (without -O) 
//
// dgOutBuf::setRecipient()
//
void dgOutBuf::setRecipient(const caNetAddr &addr)
{
	this->to = addr;
}

//
// this needs to be here (and not in dgInBufIL.h) if we
// are to avoid undefined symbols under gcc 2.7.x (without -O) 
//
// dgOutBuf::getRecipient()
//
caNetAddr dgOutBuf::getRecipient()
{
        return this->to;
}

