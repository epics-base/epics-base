/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_dbExtractArray_H
#define INC_dbExtractArray_H

#include "dbFldTypes.h"
#include "dbAddr.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void dbExtractArrayFromRec(const DBADDR *paddr, void *pto,
                                          long nRequest, long no_elements, long offset, long increment);
epicsShareFunc void dbExtractArrayFromBuf(const void *pfrom, void *pto,
                                          short field_size, short field_type,
                                          long nRequest, long no_elements, long offset, long increment);

#ifdef __cplusplus
}
#endif

#endif // INC_dbExtractArray_H
