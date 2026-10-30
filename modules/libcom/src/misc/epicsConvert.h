/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*epicsConvert.h*/

#ifndef INC_epicsConvert_H
#define INC_epicsConvert_H

#include <libComAPI.h>

#ifdef __cplusplus
extern "C" {
#endif

LIBCOM_API float epicsConvertDoubleToFloat(double value);

/* dbConvertBase is used in dbPut and dbGet string to integer conversions.
   It defaults to 0 but is set to 10 if the EPICS_DB_CONVERT_DECIMAL_ONLY
   environment variable is YES (case insensitive).
*/
LIBCOM_API extern int dbConvertBase;

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsConvert_H */
