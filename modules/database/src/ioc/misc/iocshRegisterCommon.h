/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocshRegisterCommon.h */
/* Author:  Marty Kraimer Date: 27APR2000 */

#ifndef INCiocshRegisterCommonH
#define INCiocshRegisterCommonH

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* register many useful commands */
epicsShareFunc void iocshRegisterCommon(void);

#ifdef __cplusplus
}
#endif

#endif /*INCiocshRegisterCommonH*/
