/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*epicsConvert.h*/

#ifndef INC_epicsConvert_H
#define INC_epicsConvert_H

#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc float epicsConvertDoubleToFloat(double value);

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsConvert_H */
