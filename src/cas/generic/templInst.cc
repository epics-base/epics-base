
//
// $Id$
//
// Explcit instantiation of template instances used by the server
//
//

#include "server.h"
#include "resourceLib.cc"

//
// Sun C++ 4.1 still appears to be lacking support in this area
//
#if !defined(__SUNPRO_CC)
	//
	// From Stroustrups's "The C++ Programming Language"
	// Appendix A: r.14.9 
	//
	// This explicitly instantiates the template class's member
	// functions into "templInst.o"
	//
	template class resTable <casEventMaskEntry, stringId>;
	template class resTable <casPVI, stringId>;
	template class resTable <casRes, uintId>;
#endif

