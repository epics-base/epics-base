/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author:
 * Jeffrey O. Hill
 * johill@lanl.gov
 */

#ifndef compilerSpecific_h
#define compilerSpecific_h

#ifndef __GNUC__
#   error compiler/gcc/compilerSpecific.h is only for use with the gnu compiler
#endif

#ifdef __clang__
#   error compiler/gcc/compilerSpecific.h is not for use with the clang compiler
#endif

#define EPICS_ALWAYS_INLINE __inline__ __attribute__((always_inline))

/* Expands to a 'const char*' which describes the name of the current function scope */
#define EPICS_FUNCTION __PRETTY_FUNCTION__

#ifdef __cplusplus

/*
 * in general we don't like ifdefs but they do allow us to check the
 * compiler version and make the optimistic assumption that
 * standards incompliance issues will be fixed by future compiler
 * releases
 */

/*
 * CXX_PLACEMENT_DELETE - defined if compiler supports placement delete
 */
#define CXX_PLACEMENT_DELETE

#endif /* __cplusplus */

/*
 * Enable format-string checking if possible
 */
#define EPICS_PRINTF_STYLE(f,a) __attribute__((format(__printf__,f,a)))

/*
 * Deprecation marker
 */
#define EPICS_DEPRECATED __attribute__((deprecated))

/*
 * Unused marker
 */
#define EPICS_UNUSED __attribute__((unused))

/*
 * Memory checker, checks that the function returns a valid pointer with a specific size.
 * valid meaning, the storage it points to is not already filled.
 * First argument is the function which should be called to free memory pointer, or the deallocator.
 * Second argument denotes the positional argument to which when the pointer is passed in calls to deallocator has the effect of deallocating it.
 * Remaining arguments are passed in to alloc_size, and must multiply to give the allocated size in memory.
 */
#if __GNUC__ >= 11
    #define EPICS_SIZED_MEM_CHECK(freeMem, freeMemArg, ...) __attribute__((alloc_size(__VA_ARGS__), malloc(freeMem, freeMemArg)))
#elif __GNUC__ >= 4
    #define EPICS_SIZED_MEM_CHECK(freeMem, freeMemArg, ...) __attribute__((alloc_size(__VA_ARGS__), malloc))
#elif __GNUC__ >= 3
    #define EPICS_SIZED_MEM_CHECK(freeMem, freeMemArg, ...) __attribute__((malloc))
#endif


/* 
 * Memory checker, checks that any input to a function is a non-null pointer, and that the function returns
 * a valid pointer.
 * Valid meaning, the storage it points to is not already filled.
 * Any arguments passed in should be pointers, and are checked for non-null status.
 */
#if __GNUC__
    #define EPICS_MEM_CHECK(...) __THROW __attribute_malloc__ __nonnull((__VA_ARGS__))
#endif

#endif  /* ifndef compilerSpecific_h */
