
#ifndef dgOutBufILh
#define dgOutBufILh

#include <casOpaqueAddrIL.h>
#include <outBufIL.h>

//
// dgOutBuf::dgOutBuf()
//
inline dgOutBuf::dgOutBuf (osiMutex &mutexIn, unsigned bufSizeIn) :
		outBuf(mutexIn, bufSizeIn) 
{
}

//
// dgOutBuf::getRecipient()
//
inline caAddr dgOutBuf::getRecipient()
{
	return this->to.get();
}

//
// dgOutBuf::setRecipient()
//
inline void dgOutBuf::setRecipient(const caAddr &addr)
{
	this->to.set(addr);
}

//
// dgOutBuf::clear()
//
inline void dgOutBuf::clear()
{
	this->to.clear();
	this->outBuf::clear();
}

#endif // dgOutBufILh

