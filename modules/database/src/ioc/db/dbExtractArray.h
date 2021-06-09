/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_dbExtractArray_H
#define INC_dbExtractArray_H

#include "dbFldTypes.h"
#include "dbAddr.h"
#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Make a copy of parts of an array.
 *
 * The source array may or may not be a record field.
 *
 * The increment parameter is used to support array filters; it
 * means: copy only every increment'th element, starting at offset.
 *
 * The offset and no_elements parameters are used to support the
 * circular buffer feature of record fields: elements before offset
 * are treated as if they came right after no_elements.
 *
 * This function does not do any conversion on the array elements.
 *
 * Preconditions:
 *   nRequest >= 0, no_elements >= 0, increment > 0
 *   0 <= offset < no_elements
 *   pto points to a buffer with at least field_size*nRequest bytes
 *   pfrom points to a buffer with exactly field_size*no_elements bytes
 *
 * @param pfrom         Pointer to source array.
 * @param pto           Pointer to target array.
 * @param field_size    Size of an array element.
 * @param nRequest      Number of elements to copy.
 * @param no_elements   Number of elements in source array.
 * @param offset        Wrap-around point in source array.
 * @param increment     Copy only every increment'th element.
 */
DBCORE_API void dbExtractArray(const void *pfrom, void *pto,
    short field_size, long nRequest, long no_elements, long offset,
    long increment);

#ifdef __cplusplus
}
#endif

#endif // INC_dbExtractArray_H
