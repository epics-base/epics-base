
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

