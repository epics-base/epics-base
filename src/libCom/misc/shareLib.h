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
 * Compiler specific key words to set up external symbols and entry points
 *
 * Currently this is only necessary for WIN32 DLLs and for VAXC on VMS but
 * other systems with similar requirements could be supported.
 *
 * USAGE:
 * There are two distinct classes of keywords in this file:
 *
 * 1) epicsShareAPI - specifies a multi-language calling mechanism. On windows 
 * this is the pascal calling convention which is used by visual basic and other
 * high level tools. This is only necessary if a C/C++ function needs to be called
 * from other languages or from high level tools. The epicsShareAPI keyword
 * must be present between the function's returned data type and the function's 
 * name. All compilers targeting windows accept the __stdcall keyword in this 
 * location. Functions with variable argument lists should not use the epicsShareAPI
 * keyword because __stdcall (pascal) calling convention cannot support variable
 * length ed argument lists.
 *
 * int epicsShareAPI myExtFunc ( int arg );   
 * int epicsShareAPI myExtFunc ( int arg ) {}
 *
 * ** NOTE **  epicsShareAPI is deprecated for new routines, don't use it!
 *             In a future major release (R3.15) we will remove this keyword
 *             from most EPICS APIs, although CA may continue to use it.
 *             This is a major release though, since it affects the order
 *             that arguments are pushed onto the stack on Windows and we
 *             don't want a replacement DLL to silent cause mayhem...
 *
 * 2) epicsShare{Func,Class,Extern,Def} - specifies shareable library (DLL) 
 * export/import related information in the source code. On windows these keywords 
 * allow faster dispatching of calls to DLLs because more is known at compile time. 
 * It is also not necessary to maintain a linker input files specifying the DLL
 * entry points. This maintenance can be more laborious with C++ decorated symbol 
 * names. These keywords are only necessary if the address of a function or data 
 * internal to a shareable library (DLL) needs to be visible outside of this shareable 
 * library (DLL). All compilers targeting windows accept the __declspec(dllexport)
 * and __declspec(dllimport) keywords.
 *
 * In header files declare references to externally visible variables, classes and 
 * functions like this:
 *
 * #include "shareLib.h"
 * epicsShareFunc int myExtFunc ( int arg );   
 * epicsShareExtern int myExtVar; 
 * class epicsShareClass myClass { int func ( void ); };
 *
 * In the implementation file, however, you write:
 *
 * #include <interfaces_imported_from_other_shareable_libraries.h>
 * #define epicsExportSharedSymbols
 * #include <interfaces_implemented_in_this_shareable_library.h>
 *
 * epicsShareDef int myExtVar = 4;       
 * int myExtFunc ( int arg ) {} 
 * int myClass::func ( void ) {}
 *
 * By default shareLib.h sets the DLL import / export keywords to import from
 * a DLL so that, for DLL consumers (users), nothing special must be done. However,
 * DLL implementors must set epicsExportSharedSymbols as above to specify
 * which functions are exported from the DLL and which of them are imported
 * from other DLLs.
 *
 * You must first #include what you import and then define epicsExportSharedSymbols 
 * only right before you #include the prototypes for what you implement! You must 
 * include shareLib.h again each time that the state of the import/ export keywords 
 * changes, but this usually occurs as a side effect of including the shareable
 * libraries header file(s).
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
 * epicsShareFunc int myExtFunc ( int arg );   
 * epicsShareExtern int myExtVar; 
 * class epicsShareClass myClass {};
 *
 * Fortunately, the above is only the concern of library authors and will have no 
 * impact on persons using functions and or external data from a library.
 */

#undef epicsShareExtern
#undef epicsShareDef
#undef epicsShareClass
#undef epicsShareFunc
#undef epicsShareAPI
#undef READONLY

/*
 *
 * Also check for "EPICS_DLL_NO" not defined so that we will not use these
 * keywords if it is an object library build of base under WIN32.
 */
#if defined(_WIN32) || defined(__CYGWIN32__)

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
#       if defined(_DLL) /* this indicates that we are being compiled to call DLLs */
#           define epicsShareExtern __declspec(dllimport) extern 
#           define epicsShareClass  __declspec(dllimport) 
#           define epicsShareFunc  __declspec(dllimport)
#       else
#           define epicsShareExtern extern
#           define epicsShareClass
#           define epicsShareFunc
#       endif
#   endif
#   define epicsShareDef 
#   define epicsShareAPI __stdcall /* function removes arguments */
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

#else

/* else => no import/export specifiers */

#   define epicsShareExtern extern
#   define epicsShareAPI
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
