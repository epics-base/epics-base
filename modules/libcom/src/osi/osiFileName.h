/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * osiFileName.h
 * Author: Jeff Hill
 */
#ifndef osiFileNameH
#define osiFileNameH

#include <stdlib.h>
#include <stdarg.h>

#include <libComAPI.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#  define OSI_PATH_LIST_SEPARATOR ";"
#  define OSI_PATH_SEPARATOR "\\"
#else
#  define OSI_PATH_LIST_SEPARATOR ":"
#  define OSI_PATH_SEPARATOR "/"
#endif

/** Extract directory component from path.  in-place.
 *
 * The path buffer will be modified by inserting a '\0' (nil) after
 * the last path separator.
 *
 * cf. dirname() on *NIX
 *
 * @returns The location of the added nil, or pathlen if no nil could be added.
 *
 * @since UNRELEASED
 */
LIBCOM_API
size_t epicsPathDir(char *path, size_t pathlen);

/** Extract file (and extension) component from path.  in-place.
 *
 * The path buffer will be modified to contain only the string after
 * the last path separator.
 *
 * cf. basename() on *NIX
 *
 * @returns The location of the added nil, or pathlen if no nil could be added.
 *
 * @since UNRELEASED
 */
LIBCOM_API
size_t epicsPathFile(char *path, size_t pathlen);

/** Test if path is absolute.
 * @return non-zero if absolute.  zero if relative
 *
 * @since UNRELEASED
 */
LIBCOM_API
int epicsPathIsAbs(const char *path, size_t pathlen);

/** Concatenate zero or more path fragments (followed by a NULL)
 *
 * Fragments are joined together with the OS path separator character
 * If any of the fragments is an absolute path, the preceding fragments
 * are ignored.
 *
 * NULL and empty ("") fragments are ignored.
 *
 * The special fragment @code epicsPathJoinCurDir @endcode will be substituted
 * with the current working directory (cf. epicsPathAllocCWD()).
 *
 * @returns A string buffer which must be free()'d, or NULL
 *
 * @since UNRELEASED
 */
LIBCOM_API
char* epicsPathJoin(const char **fragments, size_t nfragments);

/** Special path fragment for epicsPathJoin() which will be replaced with the current directory
 *
 * @since UNRELEASED
 */
LIBCOM_API
extern const char* const epicsPathJoinCurDir;

/** Allocate and return the current working directory.
 *
 * Called must free()
 *
 * @since UNRELEASED
 */
LIBCOM_API
char* epicsPathAllocCWD(void);

/** Return the absolute path of the current executable.
 \return NULL or the path.  Caller must free()
 *
 * @since 7.0.3.1
 */
LIBCOM_API
char *epicsGetExecName(void);

/** Return the absolute path of the directory containing the current executable.
 \return NULL or the path.  Caller must free()
 *
 * @since 7.0.3.1
 */
LIBCOM_API
char *epicsGetExecDir(void);

#ifdef __cplusplus
}
#endif

#endif /* osiFileNameH */
