
//
// $Id$
//
// Explcit instantiation of template instances used by the 
// example server
//
//

#include "exServer.h"
#include "gddAppFuncTable.cc"

//
// Sun C++ 4.1 still appears to be lacking support in this area
//
#if !defined(__SUNPRO_CC)
	//
	// From Stroustrups's "The C++ Programming Language"
	// Appendix A: r.14.9 
	//
	// This explicitly instantiates the template class's 
	// member functions into "templInst.o"
	//
	template class gddAppFuncTable <exPV>;
#endif

