
//
// $Id$
//
// Explcit instantiation of template instances used by the server
//
//

#include "server.h"
#include "resourceLib.cc"

//
// if the compiler supports explicit instantiation of
// template member functions
//
#if defined(EXPL_TEMPL)
	//
	// From Stroustrups's "The C++ Programming Language"
	// Appendix A: r.14.9 
	//
	// This explicitly instantiates the template class's member
	// functions into "templInst.o"
	//
	template class resTable <casEventMaskEntry, stringId>;
	template class resTable <casRes, uintId>;
#endif

