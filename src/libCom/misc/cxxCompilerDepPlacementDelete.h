
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

#ifndef cxxCompilerDepPlacementDelete_h
#define cxxCompilerDepPlacementDelete_h

// This file tells us if the C++ compiler supports placement delete. Since this is
// a compiler, and not os dependent problem, then ifdefs are used. The ifdefs allow 
// us to assume that these problems will get fixed by future compiler releases.

#if defined ( _MSC_VER )
#   if _MSC_VER >= 1200 // visual studio 6.0 or later 
#       define CXX_PLACEMENT_DELETE
#   endif
#elif defined ( __HP_aCC ) 
#   if _HP_aCC > 033300
#       define CXX_PLACEMENT_DELETE
#   endif
#elif defined ( __BORLANDC__ ) 
#   if __BORLANDC__ > 0x550
#       define CXX_PLACEMENT_DELETE
#   endif
#elif defined ( __GNUC__ ) 
#   if __GNUC__ > 2 || ( __GNUC__ == 2 && __GNUC_MINOR__ >= 95 )
#       define CXX_PLACEMENT_DELETE
#   endif
#else
#   define CXX_PLACEMENT_DELETE
#endif

#endif // ifndef cxxCompilerDepPlacementDelete_h
