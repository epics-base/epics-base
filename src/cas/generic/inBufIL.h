
#ifndef inBufILh
#define inBufILh

//
// inBuf::bytesPresent()
//
inline bufSizeT inBuf::bytesPresent () const 
{
	return this->bytesInBuffer-this->nextReadIndex;
}

//
// inBuf::bytesAvailable()
//
inline bufSizeT inBuf::bytesAvailable () const 
{
	bufSizeT bp;
	bp = this->bytesPresent();
	bp += this->incommingBytesPresent();
	return bp;
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
// inBuf::clear()
//
inline void inBuf::clear ()
{
	this->bytesInBuffer = 0u;
	this->nextReadIndex = 0u;
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
inline void inBuf::removeMsg (bufSizeT nBytes) 
{
	this->nextReadIndex += nBytes;
	assert (this->nextReadIndex<=this->bytesInBuffer);
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

#endif // inBufILh

