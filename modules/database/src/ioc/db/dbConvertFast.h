/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/** @file dbConvertFast.h
 *  @brief Data conversion for scalar values
 *
 * The typedef FASTCONVERTFUNC is defined in link.h as:
 * @code
 *   long convert(const void *from, void *to, const struct dbAddr *paddr);
 * @endcode
 *
 * The arrays declared here provide pointers to the fast conversion
 * routine where the first array index is the data type for the first
 * "from" pointer arg, and the second array index is the data type for
 * the second "to" pointer arg. The array index values are a subset of
 * the DBF_ enum values defined in dbFldTypes.h
 */

#ifndef INCdbConvertFasth
#define INCdbConvertFasth

#include "dbFldTypes.h"
#include "dbCoreAPI.h"
#include "link.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Function pointers for get conversions */
DBCORE_API extern const FASTCONVERTFUNC
    dbFastGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1];
/** Function pointers for put conversions */
DBCORE_API extern const FASTCONVERTFUNC
    dbFastPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1];

#ifdef __cplusplus
}
#endif

#endif /*INCdbConvertFasth*/
