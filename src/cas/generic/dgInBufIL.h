
#ifndef dgInBufILh
#define dgInBufILh

#include <casOpaqueAddrIL.h>
#include <inBufIL.h>

//
// dgInBuf::clear()
//
inline void dgInBuf::clear()
{
	this->from.clear();
	this->inBuf::clear();
}

#endif // dgInBufILh

