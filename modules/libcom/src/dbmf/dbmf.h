/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file dbmf.h
 * \author Jim Kowalkowski, Marty Kraimer
 *
 * \brief A library to manage storage that is allocated and quickly freed.
 *
 * Database Macro/Free describes a facility that prevents memory fragmentation
 * when temporary storage is being allocated and freed a short time later, at
 * the same time that much longer-lived storage is also being allocated, such
 * as when parsing items for the IOC database while also creating records.
 *
 * Routines whin iocCore like dbLoadDatabase() have the following attributes:
 * 	- They repeatedly call malloc() followed soon afterwards by a call to
 * 	free() the temporarily allocated storage.
 * 	- Between those calls to malloc() and free(), additional calls to
 * 	malloc() are made that do NOT have an associated free().
 *
 * \note In some environment, e.g. vxWorks, this behavior causes severe memory
 * fragmentation.
 *
 * \note This facility should NOT be used by code that allocates storage and
 * then keeps it for a considerable period of time before releasing. Such code
 * should consider using the freeList library.
 */
#ifndef DBMF_H
#define DBMF_H

#include <stdlib.h>
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Initialize the facility
 * \param size The maximum size request from dbmfMalloc() that will be
 * allocated from the dbmf pool (Size is always made a multiple of 8).
 * \param chunkItems Each time malloc() must be called size*chunkItems bytes
 * are allocated.
 * \return 0 on success, -1 if already initialized
 *
 * \note If dbmfInit() is not called before one of the other routines then it
 * is automatically called with size=64 and chunkItems=10
 */
LIBCOM_API int dbmfInit(size_t size, int chunkItems);
/**
 * \brief Allocate memory.
 * \param bytes If bytes > size then malloc() is used to allocate the memory.
 * \return Pointer to the newly-allocated memory, or NULL on failure.
 */
LIBCOM_API void * dbmfMalloc(size_t bytes);
/**
 * \brief Duplicate a string.
 *
 * Create a copy of the input string.
 * \param str Pointer to the null-terminated string to be copied.
 * \return A pointer to the new copy, or NULL on failure.
 */
LIBCOM_API char * dbmfStrdup(const char *str);
/**
 * \brief Duplicate a string (up to len bytes).
 *
 * Copy at most len bytes of the input string into a new buffer. If no nil
 * terminator is seen in the first \c len bytes a nil terminator is added.
 * \param str Pointer to the null-terminated string to be copied.
 * \param len Max number of bytes to copy.
 * \return A pointer to the new string, or NULL on failure.
 */
LIBCOM_API char * dbmfStrndup(const char *str, size_t len);
/**
 * \brief Concatenate three strings.

 * Returns a pointer to a null-terminated string made by concatenating the
 * three input strings.
 * \param lhs Start string to which the others get concatenated to (left part).
 * \param mid Next string to be concatenated to the lhs (mid part).
 * \param rhs Last string to be concatenated to the lhs+mid (right part).
 * \return A pointer to the new string, or NULL on failure.
 */
LIBCOM_API char * dbmfStrcat3(const char *lhs, const char *mid,
    const char *rhs);
/**
 * \brief Free the memory allocated by dbmfMalloc.
 * \param bytes Pointer to memory obtained from dbmfMalloc(), dbmfStrdup(),
 * dbmfStrndup() or dbmfStrcat3().
 */
LIBCOM_API void dbmfFree(void *bytes);
/**
 * \brief Free all chunks that contain only free items.
 */
LIBCOM_API void dbmfFreeChunks(void);
/**
 * \brief Show the status of the dbmf memory pool.
 * \param level Detail level.
 * \return 0.
 */
LIBCOM_API int dbmfShow(int level);

#ifdef __cplusplus
}
#endif

#endif
