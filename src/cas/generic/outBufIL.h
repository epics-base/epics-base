/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef outBufILh
#define outBufILh

#ifdef epicsExportSharedSymbols
#define outBufILh_epicsExportSharedSymbols
#undef epicsExportSharedSymbols
#endif

#include "epicsGuard.h"

#ifdef outBufILh_epicsExportSharedSymbols
#define epicsExportSharedSymbols
#endif


//
// outBuf::bytesPresent ()
// number of bytes in the output queue
//
inline bufSizeT outBuf::bytesPresent () const
{
    //
    // Note on use of lock here:
    // This guarantees that any pushCtx() operation
    // in progress completes before another thread checks.
    //
    epicsGuard < epicsMutex > locker ( this->mutex );
    bufSizeT result = this->stack;
	return result;
}

//
// outBuf::clear ()
//
inline void outBuf::clear ()
{
    //
    // Note on use of lock here:
    // This guarantees that any pushCtx() operation
    // in progress completes before another thread 
    // clears.
    //
    epicsGuard < epicsMutex > locker ( this->mutex );
	this->stack = 0u;
}

//
//	outBuf::allocMsg () 
//
//	allocates space in the outgoing message buffer
//
//	(if space is avilable this leaves the send lock applied)
//			
//inline caStatus outBuf::allocMsg (bufSizeT extsize, caHdr **ppMsg)
//{
//    return this->allocRawMsg (extsize + sizeof(caHdr), (void **)ppMsg);
//}

//
// outBuf::commitRawMsg()
//
inline void outBuf::commitRawMsg (bufSizeT size)
{
	this->stack += size;
	assert ( this->stack <= this->bufSize );
	
    this->mutex.unlock();
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

#endif // outBufILh

