
#ifndef outBufILh
#define outBufILh

//
// outBuf::bytesPresent()
// number of bytes in the output queue
//
inline bufSizeT outBuf::bytesPresent() const
{
	return this->stack;
}

//
// outBuf::bytesFree()
// number of bytes unused in the output queue
//
inline bufSizeT outBuf::bytesFree() const
{
	return this->bufSize - this->stack;
}

//
// outBuf::clear()
//
inline void outBuf::clear()
{
	this->stack = 0u;
}

#endif // outBufILh

