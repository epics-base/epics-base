
#include <casInternal.h>
#include <>


casOpaqueAddr::checkSize sizeChecker;

checkSize::checkSize()
{
	assert( sizeof(casOpaqueAddr::opaqueAddr) >= sizeof(caAddr));
}

