/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * Use compiler specific key words to set up shareable library
 * external symbols and entry points
 *
 * Right now this is only necessary for WIN32 DLL, and to a lesser extent VAXC.
 * The approach should be general enough for future systems, however.
 *
 *
 * USAGE:
 *
 * In header files, declare variables, classes and functions
 * to be __exported__ like this:
 *
 *  epicsShareFunc int epicsShareAPI 
 *          a_func (int arg);   function prototype
 * or
 *  epicsShareFunc int epicsShareAPIV 
 *          a_func (int arg, ...);  variable args function prototype
 *                                                (using either ... or va_list)
 *  epicsShareExtern  int a_var;        reference variable
 *                                                (reference declaration)
 *  epicsShareDef int a_var= 4;         create variable instance
 *                                                (definition declaration)
 *  class epicsShareClass a_class;      reference a class 
 *
 * Usually the epicsShare... macros expand to
 *  "import this from a DLL"  (on WIN32, on Unix it's a NOOP)
 *
 * In the implementation file, however, you write:
 *
 * #include <routines_imported.h>
 * #include <routines_imported_from_other_dlls.h>
 * #define epicsExportSharedSymbols
 * ! no more includes specifying routines outside this DLLs from here on ! 
 * #include <in_this_dll_but_not_implemented_in_this_file.h>
 * #include <what_I_implement_in_this_file.h>
 *
 * The point is: define epicsExportSharedSymbols only
 * right before you include the prototypes for what you implement!
 * You must include shareLib.h again because this is where they get changed,
 * but this usually occurs as a side effect of including the header file.
 * This time the epicsShare... macros expand to
 *  "export this from the DLL that we are building". (again only on WIN32)
 *
 * Frequently a header file for a shareable library exported interface will
 * have some preprocessor switches like this if this header file must also  
 * include header files describing interfaces to other shareable libraries.
 *
 * #ifdef epicsExportSharedSymbols
 * #   define interfacePDQ_epicsExportSharedSymbols
 * #   undef epicsExportSharedSymbols
 * #endif
 *
 * #include "epicsTypes.h"
 * #include "epicsTime.h"
 *
 * #ifdef interfacePDQ_epicsExportSharedSymbols
 * #   define epicsExportSharedSymbols
 * #   include "shareLib.h"
 * #endif
 *
 * Alternatively, you can force header files describing interfaces outside of your 
 * library to be included before setting epicsExportSharedSymbols by including them
 * before setting epicsExportSharedSymbols. Since all well written header files have 
 * "ifdef" guards against multiple inclusion this will prevent these interfaces from 
 * being included again after epicsExportSharedSymbols is set. 
 *
 * Fortunately, the above is only the concern of library authors and will have no 
 * impact on persons using routines from a library.
 *
 * 8-22-96 -kuk-
 *
 * NOTE:
 * When we phase out support for VAXC C (and only support the ansi compliant
 * DEC C) then we can remove all epicsShareDef instances from base.
 * 
 * 1-17-02 -joh-
 */

#undef epicsShareExtern
#undef epicsShareDef
#undef epicsShareClass
#undef epicsShareFunc
#undef epicsShareAPI
#undef epicsShareAPIV
#undef READONLY

/*
 * if its WIN32 and it isnt the Cygnus GNU environment
 * (I am assuming Borlund and other MS Vis C++ competitors
 * support these MS VisC++ defacto standard keywords???? If not
 * then we should just switch on defined(_MSC_VER) here)
 *
 * Also check for "EPICS_DLL_NO" not defined so that we will not use these
 * keywords if it is an object library build of base under WIN32.
 */
#if defined(_WIN32) && !defined(__GNUC__) 

#   if defined(epicsExportSharedSymbols)
#       if defined(EPICS_DLL_NO) /* this indicates that we are not building a DLL */
#           define epicsShareExtern extern 
#           define epicsShareClass 
#           define epicsShareFunc
#       else
#           define epicsShareExtern __declspec(dllexport) extern 
#           define epicsShareClass  __declspec(dllexport) 
#           define epicsShareFunc  __declspec(dllexport)
#       endif
#   else
#       if defined(_DLL) /* this indicates that we are being compiled to call a DLL */
#           define epicsShareExtern __declspec(dllimport) extern 
#           define epicsShareClass  __declspec(dllimport) 
#           define epicsShareFunc  __declspec(dllimport)
#       else
#           define epicsShareExtern extern
#           define epicsShareClass
#           define epicsShareFunc
#       endif
#   endif
    /*
     * Subroutine removes arguments 
     * (Bill does not allow __stdcall to be next to
     * __declspec(xxxx))
     */
#   define epicsShareAPI __stdcall
    /*
     * Variable args functions cannot be __stdcall
     * Use this for variable args functions
     * (Those using either ... or va_list arguments)
     */
#   define epicsShareAPIV __cdecl

#   define epicsShareDef 
#   define READONLY const
/*
 * if its the old VAX C Compiler (not DEC C)
 */
#elif defined(VAXC)

    /* 
     * VAXC creates FORTRAN common blocks when
     * we use "extern int fred"/"int fred=4". Therefore,
     * the initialization is not loaded unless we
     * call a function in that object module.
     *
     * DEC CXX does not have this problem.
     * We suspect (but do not know) that DEC C 
     *  also does not have this problem.
     */
#   define epicsShareExtern globalref 
#   define epicsShareDef globaldef 
#   define READONLY const
#   define epicsShareClass
#   define epicsShareFunc
#   define epicsShareAPI
#   define epicsShareAPIV

#else

/* else => no import/export specifiers */

#   define epicsShareExtern extern
#   define epicsShareAPI
#   define epicsShareAPIV
#   define epicsShareClass
#   define epicsShareDef

#   define epicsShareFunc
#   if defined(__STDC__) || defined (__cplusplus)
#       define READONLY const
#   else
#       define READONLY
#   endif

#endif

#ifndef INLINE_defs_EPICS
#define INLINE_defs_EPICS
#   ifndef __cplusplus
#       if defined (__GNUC__)
#           define INLINE static __inline__
#       elif defined (_MSC_VER)
#           define INLINE __inline
#       elif defined (_SUNPRO_C)
#           pragma error_messages (off, E_EXTERN_PRIOR_REDECL_STATIC)
#           define INLINE static
#       else
#           define INLINE static
#       endif
#   endif /* ifndef __cplusplus */
#endif /* ifdef INLINE_defs_EPICS */


