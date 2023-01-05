/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/**
 * \file shareLib.h
 * \brief Mark external symbols and entry points for shared libraries.
 *
 * This is the header file for the "decorated names" that appear in
 * header files, e.g.
 *
 *     #define epicsExportSharedSymbols
 *     epicsShareFunc int epicsShareAPI a_func(int arg);
 *
 * These are needed to properly create DLLs on MS Windows.
 *
 * \note This header file is deprecated. A newer mechanism is available that
 * automatically handles the differences in shared libraries between Linux and
 * Windows while avoiding the pitfalls of the `epicsExportSharedSymbols` macro.
 * If you are implementing a library, refer to the documentation within
 * [**makeAPIheader.pl**](makeAPIheader.html) or run `makeAPIheader.pl -h`.
 *
 * ### USAGE
 *
 * There are two distinct classes of keywords in this file:
 *
 * -# epicsShareAPI - specifies a multi-language calling mechanism. On windows
 * this is the pascal calling convention which is used by visual basic and other
 * high level tools. This is only necessary if a C/C++ function needs to be called
 * from other languages or from high level tools. The epicsShareAPI keyword
 * must be present between the function's returned data type and the function's
 * name. All compilers targeting windows accept the __stdcall keyword in this
 * location. Functions with variable argument lists should not use the epicsShareAPI
 * keyword because __stdcall (pascal) calling convention cannot support variable
 * length ed argument lists.
 *
 *     int epicsShareAPI myExtFunc ( int arg );
 *     int epicsShareAPI myExtFunc ( int arg ) {}
 *
 *   \note  The epicsShareAPI attribute is deprecated and has been removed
 *             from all IOC-specific APIs.  Most libCom APIs still use it, but
 *             it may get removed from these at some point in the future.
 *
 * -# epicsShare{Func,Class,Extern,Def} - specifies shareable library (DLL)
 * export/import related information in the source code. On windows these keywords
 * allow faster dispatching of calls to DLLs because more is known at compile time.
 * It is also not necessary to maintain a linker input files specifying the DLL
 * entry points. This maintenance can be more laborious with C++ decorated symbol
 * names. These keywords are only necessary if the address of a function or data
 * internal to a shareable library (DLL) needs to be visible outside of this shareable
 * library (DLL). All compilers targeting windows accept the __declspec(dllexport)
 * and __declspec(dllimport) keywords. For GCC version 4 and above the first three
 * keywords specify a visibility attribute of "default", which marks the symbol as
 * exported even when gcc is given the option -fvisibility=hidden. Doing this can
 * significantly reduce the number of symbols exported to a shared library. See the
 * URL below for more information.
 *
 * In header files declare references to externally visible variables, classes and
 * functions like this:
 *
 *     #include "shareLib.h"
 *     epicsShareFunc int myExtFunc ( int arg );
 *     epicsShareExtern int myExtVar;
 *     class epicsShareClass myClass { int func ( void ); };
 *
 * In the implementation file, however, you write:
 *
 *     #include <interfaces_imported_from_other_shareable_libraries.h>
 *     #define epicsExportSharedSymbols
 *     #include <interfaces_implemented_in_this_shareable_library.h>
 *
 *     epicsShareDef int myExtVar = 4;
 *     int myExtFunc ( int arg ) {}
 *     int myClass::func ( void ) {}
 *
 * By default shareLib.h sets the DLL import / export keywords to import from
 * a DLL so that, for DLL consumers (users), nothing special must be done. However,
 * DLL implementers must set epicsExportSharedSymbols as above to specify
 * which functions are exported from the DLL and which of them are imported
 * from other DLLs.
 *
 * You must first \c \#include what you import and then \c \#define
 * \c epicsExportSharedSymbols only right before you \c \#include the
 * prototypes for what you implement! You must include shareLib.h again each
 * time that the state of the import/export keywords changes, but this
 * usually occurs as a side effect of including the shareable libraries header
 * file(s).
 *
 * ### Deprecated Usage
 *
 * The construct described below is used in some EPICS header files bit is
 * no longer recommended as it makes it difficult to diagnose the incorrect
 * inclusion of headers that are the wrong side of an \c epicsExportSharedSymbols
 * marker.
 *
 * Sometimes a header file for a shareable library exported interface will
 * have some preprocessor switches like this if this header file must also
 * include header files describing interfaces to other shareable libraries.
 *
 *     #ifdef epicsExportSharedSymbols
 *     #   define interfacePDQ_epicsExportSharedSymbols
 *     #   undef epicsExportSharedSymbols
 *     #endif
 *
 *     #include "epicsTypes.h"
 *     #include "epicsTime.h"
 *
 *     #ifdef interfacePDQ_epicsExportSharedSymbols
 *     #   define epicsExportSharedSymbols
 *     #   include "shareLib.h"
 *     #endif
 *
 *     epicsShareFunc int myExtFunc ( int arg );
 *     epicsShareExtern int myExtVar;
 *     class epicsShareClass myClass {};
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

#if defined(_WIN32) || defined(__CYGWIN__)
/*
 * Check if EPICS_BUILD_DLL or EPICS_CALL_DLL defined and use the dllimport/
 * dllexport keywords if this is a shared library build of base under WIN32.
 */

#   if defined(epicsExportSharedSymbols)
#       if defined(EPICS_BUILD_DLL)
#           define epicsShareExtern __declspec(dllexport) extern
#           define epicsShareClass  __declspec(dllexport)
#           define epicsShareFunc   __declspec(dllexport)
#       else
#           define epicsShareExtern extern
#           define epicsShareClass
#           define epicsShareFunc
#       endif
#   else
#       if defined(EPICS_CALL_DLL)
#           define epicsShareExtern __declspec(dllimport) extern
#           define epicsShareClass  __declspec(dllimport)
#           define epicsShareFunc   __declspec(dllimport)
#       else
#           define epicsShareExtern extern
#           define epicsShareClass
#           define epicsShareFunc
#       endif
#   endif
#   define epicsShareDef
#   define epicsShareAPI __stdcall /* function removes arguments */
#   define READONLY const

#elif __GNUC__ >= 4
/*
 * See http://gcc.gnu.org/wiki/Visibility
 * For these to work, gcc must be given the flag
 *     -fvisibility=hidden
 * and g++ the flags
 *     -fvisibility=hidden -fvisibility-inlines-hidden
 */

#   define epicsShareExtern __attribute__ ((visibility("default"))) extern
#   define epicsShareClass __attribute__ ((visibility("default")))
#   define epicsShareFunc __attribute__ ((visibility("default")))

#   define epicsShareDef
#   define epicsShareAPI
#   if defined(__STDC__) || defined (__cplusplus)
#       define READONLY const
#   else
#       define READONLY
#   endif

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
