
#include <server.h>
#include <casOpaqueAddrIL.h>

//
// dgInBuf::~dgInBuf()
// (force virtual constructor)
//
dgInBuf::~dgInBuf()
{
}

//
// dgInBuf::xRecv ()
//
xRecvStatus dgInBuf::xRecv (char *pBufIn, bufSizeT nBytesToRecv,
        bufSizeT &nByesRecv)
{
        caAddr addr;
        xRecvStatus stat;
 
        stat = this->xDGRecv (pBufIn, nBytesToRecv, nByesRecv, addr);
        if (stat == xRecvOK) {
                this->from.set(addr);
        }
        return stat;
}

