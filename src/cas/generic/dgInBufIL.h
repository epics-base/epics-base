
#ifndef dgInBufILh
#define dgInBufILh

#include <casOpaqueAddrIL.h>
#include <inBufIL.h>

//
// dgInBuf::dgInBuf()
//
inline dgInBuf::dgInBuf (osiMutex &mutexIn, unsigned bufSizeIn) :
		inBuf(mutexIn, bufSizeIn) 
{
}

//
// dgInBuf::getSender()
//
inline const caAddr dgInBuf::getSender() const
{
	return this->from.get();
}

//
// dgInBuf::hasAddress()
//
inline int dgInBuf::hasAddress() const
{
	return this->from.hasBeenInitialized();
}

//
// dgInBuf::clear()
//
inline void dgInBuf::clear()
{
	this->from.clear();
	this->inBuf::clear();
}

#endif // dgInBufILh

