/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**\file osiPoolStatus.h
 * \author Jeff Hill
 * \brief Functions to check the state of the system memory pool.
 *
 **/

#ifndef INC_osiPoolStatus_H
#define INC_osiPoolStatus_H

#include <stdlib.h>
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/**\brief Checks if a memory block of a specific size can be safely allocated.
 *
 * The meaning of "safely allocated" is target-specific, some additional free
 * space is usually necessary to keep the system running reliably.
 * On vxWorks this returns True if at least 100000 bytes is free.
 *
 * \note This routine is called quite frequently by the IOC so an efficient
 * implementation is important.
 *
 * \param contiguousBlockSize Block size to check.
 * \return True if the requested memory should be available.
 */
LIBCOM_API int epicsStdCall osiSufficentSpaceInPool ( size_t contiguousBlockSize );

#ifdef __cplusplus
}
#endif

#include "osdPoolStatus.h"

#endif /* INC_osiPoolStatus_H */
