/*************************************************************************\
* Copyright (c) 2017 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef LIBCOMVERSION_H
#define LIBCOMVERSION_H

#include <epicsVersion.h>
#include <shareLib.h>

#ifndef VERSION_INT
#  define VERSION_INT(V,R,M,P) ( ((V)<<24) | ((R)<<16) | ((M)<<8) | (P))
#endif

/* include generated headers with:
 *   EPICS_LIBCOM_MAJOR_VERSION
 *   EPICS_LIBCOM_MINOR_VERSION
 *   EPICS_LIBCOM_MAINTENANCE_VERSION
 *   EPICS_LIBCOM_DEVELOPMENT_FLAG
 */
#include "libComVersionNum.h"

#define LIBCOM_VERSION_INT VERSION_INT(EPICS_LIBCOM_MAJOR_VERSION, EPICS_LIBCOM_MINOR_VERSION, EPICS_LIBCOM_MAINTENANCE_VERSION, 0)

#endif // LIBCOMVERSION_H
