
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

// Author: 
// Jeffrey O. Hill
// johill@lanl.gov

#ifndef cxxCompilerDependencies_h
#define cxxCompilerDependencies_h

// This  tells us what features of standard C++ a compiler supports.
// Since this is a compiler, and not os dependent issue, then ifdefs 
// are used. The ifdefs allow us to assume that these problems will 
// get fixed by future compiler releases.
//
// CXX_PLACEMENT_DELETE - defined if compiler supports placement delete
// CXX_THROW_SPECIFICATION - defined if compiler supports throw specification
//

#if defined ( _MSC_VER )
#   if _MSC_VER >= 1200 // visual studio 6.0 or later 
#       define CXX_PLACEMENT_DELETE
#   endif
#   if _MSC_VER > 1300 // some release after visual studio 7 we hope 
#       define CXX_THROW_SPECIFICATION
#   endif
#elif defined ( __HP_aCC ) 
#   if _HP_aCC > 033300
#       define CXX_PLACEMENT_DELETE
#   endif
#   define CXX_THROW_SPECIFICATION
#elif defined ( __BORLANDC__ ) 
#   if __BORLANDC__ > 0x550
#       define CXX_PLACEMENT_DELETE
#   endif
#   define CXX_THROW_SPECIFICATION
#elif defined ( __GNUC__ ) 
#   if __GNUC__ > 2 || ( __GNUC__ == 2 && __GNUC_MINOR__ >= 95 )
#       define CXX_PLACEMENT_DELETE
#       define CXX_THROW_SPECIFICATION
#   endif
#else
#   define CXX_PLACEMENT_DELETE
#   define CXX_THROW_SPECIFICATION
#endif

// usage: void func () epics_throws (( std::bad_alloc, std::logic_error ))
#if defined ( CXX_THROW_SPECIFICATION )
#   define epics_throws(X) throw X
#else
#   define epics_throws(X)
#endif

#endif // ifndef cxxCompilerDependencies_h
