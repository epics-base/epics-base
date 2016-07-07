/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casStreamIOh
#define casStreamIOh

#include "casStrmClient.h"

struct ioArgsToNewStreamIO {
    caNetAddr clientAddr;
    SOCKET sock;
};

class casStreamIO : public casStrmClient {
public:
    casStreamIO ( caServerI &, clientBufMemoryManager &, 
        const ioArgsToNewStreamIO & );
    ~casStreamIO ();
    int getFD () const;
    void xSetNonBlocking ();
	bufSizeT inCircuitBytesPending () const;
	bufSizeT osSendBufferSize () const;
private:
    SOCKET sock;
	bufSizeT _osSendBufferSize;
    xBlockingStatus blockingFlag;

    bool sockHasBeenShutdown;
    xBlockingStatus blockingState() const;
    void osdShow ( unsigned level ) const;
    outBufClient::flushCondition osdSend ( const char *pBuf, bufSizeT nBytesReq, 
        bufSizeT & nBytesActual );
    inBufClient::fillCondition osdRecv ( char *pBuf, bufSizeT nBytesReq, 
        bufSizeT & nBytesActual );
    void forceDisconnect ();
    casStreamIO ( const casStreamIO & );
    casStreamIO & operator = ( const casStreamIO & );
};

#endif // casStreamIOh
