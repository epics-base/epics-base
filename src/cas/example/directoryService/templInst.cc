
//
// $Id$
//
// Explcit instantiation of template instances used by the 
// example server
//
//

#include "directoryServer.h"
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
	// This explicitly instantiates the template class's 
	// member functions into "templInst.o"
	//
	template class resTable <pvEntry,stringId>;
#endif

