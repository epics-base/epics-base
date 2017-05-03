/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef inBufh
#define inBufh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_inBufh
#   undef epicsExportSharedSymbols
#endif

#undef epicsAssertAuthor
#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"
#include "epicsAssert.h"

#ifdef epicsExportSharedSymbols_inBufh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "clientBufMemoryManager.h"

class inBufCtx {
    friend class inBuf;
public:
    enum pushCtxResult { pushCtxNoSpace, pushCtxSuccess };
    inBufCtx ( const class inBuf & ); // success
    inBufCtx (); // failure
    
    pushCtxResult pushResult () const;
    
private:
    pushCtxResult stat;
    char * pBuf;
    bufSizeT bufSize;
    bufSizeT bytesInBuffer;
    bufSizeT nextReadIndex;
};

class inBufClient {
public:
    enum fillCondition { casFillNone,  casFillProgress,  
        casFillDisconnect };
    // this is a hack for a Solaris IP kernel feature
    enum fillParameter { fpNone, fpUseBroadcastInterface };
    virtual unsigned getDebugLevel () const = 0;
    virtual fillCondition xRecv ( char *pBuf, bufSizeT nBytesToRecv, 
        enum fillParameter parm, bufSizeT &nByesRecv ) = 0;
    virtual void hostName ( char *pBuf, unsigned bufSize ) const = 0;
protected:
    virtual ~inBufClient() {}
};

class inBuf {
    friend class inBufCtx;
public:
    inBuf ( class inBufClient &, class clientBufMemoryManager &, 
        bufSizeT ioMinSizeIn );
    virtual ~inBuf ();
    bufSizeT bytesPresent () const;
    bool full () const;
    inBufClient::fillCondition fill ( 
        inBufClient::fillParameter parm = inBufClient::fpNone );  
    void show ( unsigned level ) const;
    void removeMsg ( bufSizeT nBytes );
    char * msgPtr () const;
    //
    // This is used to create recursive protocol stacks. A subsegment
    // of the buffer of max size "maxSize" is assigned to the next
    // layer down in the protocol stack by pushCtx () until popCtx ()
    // is called. The roiutine popCtx () returns the actual number
    // of bytes used by the next layer down.
    //
    // pushCtx() returns an outBufCtx to be restored by popCtx()
    //
    const inBufCtx pushCtx ( bufSizeT headerSize, bufSizeT bodySize );
	bufSizeT popCtx ( const inBufCtx & ); // returns actual size
    bufSizeT bufferSize () const;
    void expandBuffer (bufSizeT needed);
private:
    class inBufClient & client;
    class clientBufMemoryManager & memMgr;
    char * pBuf;
    bufSizeT bufSize;
    bufSizeT bytesInBuffer;
    bufSizeT nextReadIndex;
    bufSizeT ioMinSize;
    unsigned ctxRecursCount;
	inBuf ( const inBuf & );
	inBuf & operator = ( const inBuf & );
};

//
// inBuf::bytesPresent()
//
inline bufSizeT inBuf::bytesPresent () const 
{
	return this->bytesInBuffer - this->nextReadIndex;
}

//
// inBuf::full()
//
inline bool inBuf::full () const
{
	if (this->bufSize-this->bytesPresent()<this->ioMinSize) {
		return true;
	}
	return false;
}

//
// inBuf::msgPtr()
//
inline char *inBuf::msgPtr () const 
{
	return &this->pBuf[this->nextReadIndex];
}

//
// inBuf::removeMsg()
//
inline void inBuf::removeMsg ( bufSizeT nBytes ) 
{
	this->nextReadIndex += nBytes;
	assert ( this->nextReadIndex <= this->bytesInBuffer );
}

//
// inBufCtx::inBufCtx ()
//
inline inBufCtx::inBufCtx () :
    stat (pushCtxNoSpace) {}

//
// inBufCtx::inBufCtx ()
//
inline inBufCtx::inBufCtx (const inBuf &inBufIn) :
    stat (pushCtxSuccess), pBuf (inBufIn.pBuf), 
    bufSize (inBufIn.bufSize), bytesInBuffer (inBufIn.bytesInBuffer),
    nextReadIndex (inBufIn.nextReadIndex) {}

//
// inBufCtx::pushResult
// 
inline inBufCtx::pushCtxResult inBufCtx::pushResult () const
{
    return this->stat;
}

#endif // inBufh

