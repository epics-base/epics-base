
#ifndef outBufILh
#define outBufILh

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
    this->mutex.lock ();
    bufSizeT result = this->stack;
    this->mutex.unlock ();
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
    this->mutex.lock ();
	this->stack = 0u;
    this->mutex.unlock ();
}

//
//	outBuf::allocMsg () 
//
//	allocates space in the outgoing message buffer
//
//	(if space is avilable this leaves the send lock applied)
//			
inline caStatus outBuf::allocMsg (bufSizeT extsize, caHdr **ppMsg)
{
    return this->allocRawMsg (extsize + sizeof(caHdr), (void **)ppMsg);
}

//
// outBuf::commitRawMsg()
//
inline void outBuf::commitRawMsg (bufSizeT size)
{
	this->stack += size;
	assert (this->stack <= this->bufSize);
	
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

