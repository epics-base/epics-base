
#include "casdef.h"

//
// this needs to be here (and not in casOpaqueAddrIL.h) if we
// are to avoid undefined symbols under gcc 2.7.x without -O
//
//
// casOpaqueAddr::casOpaqueAddr()
//
casOpaqueAddr::casOpaqueAddr()
{
	this->clear();
}

//
// this needs to be here (and not in casOpaqueAddrIL.h) if we
// are to avoid undefined symbols under gcc 2.7.x without -O
//
//
// casOpaqueAddr::clear()
//
void casOpaqueAddr::clear()
{
	this->init = 0;
}


