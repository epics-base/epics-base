// $Id$
//	Author: Andrew Johnson
//	Date:   February 2001

// This file exists to try and provide a semblance of a standard
// C++ environment on all the compilers we support.  GCC versions
// earlier than 3.0 don't put things in the std:: namespace even
// where the compiler actually supports namespaces, while other
// compilers do. The STD_ macro is therefor defined to be std::
// where needed, empty where not.  Eventually we'll be able to
// do a s/STD_ /std::/g and get rid of this header...

#ifndef __EPICS_CPP_STD_H__
#define __EPICS_CPP_STD_H__

#if defined(__GNUC__) && (__GNUC__<3)

  #define STD_

#else

  #define STD_ std::

#endif

#endif // __EPICS_CPP_STD_H__
