
#include <server.h>
#include <casOpaqueAddrIL.h>

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
        return xDGSend(pBufIn, nBytesAvailableToSend, nBytesSent,
                        this->to.get());
}

