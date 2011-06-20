
/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * Author: 
 * Jeffrey O. Hill
 * johill@lanl.gov
 */

#ifndef compilerDependencies_h
#define compilerDependencies_h

/*
 * This is an attempt to move all tests identifying what features a 
 * compiler supports into one file.
 *
 * Since this is a compiler, and not os dependent, issue then ifdefs 
 * are used. The ifdefs allow us to make the default assumption that 
 * standards incompliance issues will be fixed by future compiler 
 * releases.
 */

#ifdef __cplusplus

/*
 * CXX_PLACEMENT_DELETE - defined if compiler supports placement delete
 * CXX_THROW_SPECIFICATION - defined if compiler supports throw specification
 */

#if defined ( _MSC_VER )
#   if _MSC_VER >= 1200  /* visual studio 6.0 or later */
#       define CXX_PLACEMENT_DELETE
#   endif
#   if _MSC_VER > 1300  /* some release after visual studio 7 we hope */
#       define CXX_THROW_SPECIFICATION
#   endif
#elif defined ( __HP_aCC ) 
#   if _HP_aCC > 33300
#       define CXX_PLACEMENT_DELETE
#   endif
#   define CXX_THROW_SPECIFICATION
#elif defined ( __BORLANDC__ ) 
#   if __BORLANDC__ >= 0x600
#       define CXX_PLACEMENT_DELETE
#   endif
#   define CXX_THROW_SPECIFICATION
#elif defined ( __GNUC__ ) 
#   if __GNUC__ > 2 || ( __GNUC__ == 2 && __GNUC_MINOR__ >= 95 )
#       define CXX_THROW_SPECIFICATION
#   endif
#   if __GNUC__ > 2 || ( __GNUC__ == 2 && __GNUC_MINOR__ >= 96 )
#       define CXX_PLACEMENT_DELETE
#   endif
#else
#   define CXX_PLACEMENT_DELETE
#   define CXX_THROW_SPECIFICATION
#endif

/*
 * usage: void func () epicsThrows (( std::bad_alloc, std::logic_error ))
 */
#if defined ( CXX_THROW_SPECIFICATION )
#   define epicsThrows(X) throw X
#else
#   define epicsThrows(X)
#endif

/*
 * usage: epicsPlacementDeleteOperator (( void *, myMemoryManager & ))
 */
#if defined ( CXX_PLACEMENT_DELETE )
#   define epicsPlacementDeleteOperator(X) void operator delete X;
#else
#   define epicsPlacementDeleteOperator(X)
#endif

#endif /* __cplusplus */

/*
 * Enable format-string checking if possible
 */
#ifdef __GNUC__
# define EPICS_PRINTF_STYLE(f,a) __attribute__((format(__printf__,f,a)))
#else
# define EPICS_PRINTF_STYLE(f,a)
#endif

/*
 * Deprecation marker
 */
#if defined( __GNUC__ ) && (__GNUC__ > 2)
# define EPICS_DEPRECATED __attribute__((deprecated))
#else
# define EPICS_DEPRECATED
#endif

#endif  /* ifndef compilerDependencies_h */
