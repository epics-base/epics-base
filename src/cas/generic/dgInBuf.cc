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
#include "inBufIL.h"

//
// this needs to be here (and not in dgInBufIL.h) if we
// are to avoid undefined symbols under gcc 2.7.x without -O
//
// dgInBuf::dgInBuf ()
//
dgInBuf::dgInBuf (osiMutex &mutexIn, unsigned bufSizeIn) :
                inBuf(mutexIn, bufSizeIn)
{
}

//
// dgInBuf::~dgInBuf()
// (force virtual constructor)
//
dgInBuf::~dgInBuf()
{
}

//
// this needs to be here (and not in dgInBufIL.h) if we
// are to avoid undefined symbols under gcc 2.7.x without -O
//
// dgInBuf::hasAddress()
//
int dgInBuf::hasAddress() const
{
	return this->from.isSock();
}

//
// dgInBuf::xRecv ()
//
xRecvStatus dgInBuf::xRecv (char *pBufIn, bufSizeT nBytesToRecv,
        bufSizeT &nByesRecv)
{
        caNetAddr addr;
        xRecvStatus stat;
 
        stat = this->xDGRecv (pBufIn, nBytesToRecv, nByesRecv, addr);
        if (stat == xRecvOK) {
                this->from = addr;
        }
        return stat;
}

//
// dgInBuf::getSender()
//
caNetAddr dgInBuf::getSender() const
{
        return this->from;
}

