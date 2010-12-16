/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef outBufh
#define outBufh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_outBufh
#   undef epicsExportSharedSymbols
#endif

#include "caProto.h"

#ifdef epicsExportSharedSymbols_outBufh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "casdef.h"
#include "clientBufMemoryManager.h"

//
// outBufCtx
//
class outBufCtx {
    friend class outBuf;
public:
    enum pushCtxResult { pushCtxNoSpace, pushCtxSuccess };
    outBufCtx ( const class outBuf & ); // success
    outBufCtx (); // failure

    pushCtxResult pushResult () const;

private:
    pushCtxResult stat;
	char * pBuf;
	bufSizeT bufSize;
	bufSizeT stack;
};

class outBufClient {            // X aCC 655
public:
    enum flushCondition { 
        flushNone = 0, 
        flushProgress = 1, 
        flushDisconnect = 2 
    };
    static const char * ppFlushCondText[3];
	virtual unsigned getDebugLevel () const = 0;
	virtual void sendBlockSignal () = 0;
	virtual flushCondition xSend ( char *pBuf, bufSizeT nBytesToSend, 
		                            bufSizeT & nBytesSent ) = 0;
	virtual void hostName ( char *pBuf, unsigned bufSize ) const = 0;
    virtual bufSizeT osSendBufferSize () const = 0;
protected:
    virtual ~outBufClient() {}
};

//
// outBuf
//
class outBuf {
    friend class outBufCtx;
public:
	outBuf ( outBufClient &, clientBufMemoryManager & );
	virtual ~outBuf ();

	bufSizeT bytesPresent () const;

	//
	// flush output queue
	// (returns the number of bytes sent)
    //
    outBufClient::flushCondition flush ();

	void show (unsigned level) const;

    unsigned bufferSize () const;

	//
	// allocate message buffer space
	// (leaves message buffer locked)
	//
    caStatus copyInHeader ( ca_uint16_t response, ca_uint32_t payloadSize,
        ca_uint16_t dataType, ca_uint32_t nElem, ca_uint32_t cid, 
        ca_uint32_t responseSpecific, void **pPayload );

    //
    // commit message created with copyInHeader
    //
    void commitMsg ();
	void commitMsg ( ca_uint32_t reducedPayloadSize );

    caStatus allocRawMsg ( bufSizeT msgsize, void **ppMsg );
	void commitRawMsg ( bufSizeT size );

    //
    // This is used to create recursive protocol stacks. A subsegment
    // of the buffer of max size "maxSize" is assigned to the next
    // layer down in the protocol stack by pushCtx () until popCtx ()
    // is called. The routine popCtx () returns the actual number
    // of bytes used by the next layer down.
    //
    // pushCtx() returns an outBufCtx to be restored by popCtx()
    //
    const outBufCtx pushCtx ( bufSizeT headerSize, 
        bufSizeT maxBodySize, void *&pHeader );
	bufSizeT popCtx ( const outBufCtx & ); // returns actual size

private:
    outBufClient & client;       
    clientBufMemoryManager & memMgr;
	char * pBuf;
	bufSizeT bufSize;
	bufSizeT stack;
    unsigned ctxRecursCount;

    void expandBuffer ();

	outBuf ( const outBuf & );
	outBuf & operator = ( const outBuf & );
};

//
// outBuf::bytesPresent ()
// number of bytes in the output queue
//
inline bufSizeT outBuf::bytesPresent () const
{
	return this->stack;
}

//
// outBuf::commitRawMsg()
//
inline void outBuf::commitRawMsg (bufSizeT size)
{
	this->stack += size;
	assert ( this->stack <= this->bufSize );
}

//
// outBufCtx::outBufCtx ()
//
inline outBufCtx::outBufCtx () :
    stat (pushCtxNoSpace) {}

//
// outBufCtx::outBufCtx ()
//
inline outBufCtx::outBufCtx (const outBuf &outBufIn) :
    stat (pushCtxSuccess), pBuf (outBufIn.pBuf), 
        bufSize (outBufIn.bufSize), stack (outBufIn.stack) {}

//
// outBufCtx::pushResult
// 
inline outBufCtx::pushCtxResult outBufCtx::pushResult () const
{
    return this->stat;
}

#endif // outBufh

