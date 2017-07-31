/*************************************************************************\
* Copyright (c) 2017 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef DATABASEVERSION_H
#define DATABASEVERSION_H

#include <epicsVersion.h>

#ifndef VERSION_INT
#  define VERSION_INT(V,R,M,P) ( ((V)<<24) | ((R)<<16) | ((M)<<8) | (P))
#endif

/* include generated headers with:
 *   EPICS_DATABASE_MAJOR_VERSION
 *   EPICS_DATABASE_MINOR_VERSION
 *   EPICS_DATABASE_MAINTENANCE_VERSION
 *   EPICS_DATABASE_DEVELOPMENT_FLAG
 */
#include "databaseVersionNum.h"

#define DATABASE_VERSION_INT VERSION_INT(EPICS_DATABASE_MAJOR_VERSION, EPICS_DATABASE_MINOR_VERSION, EPICS_DATABASE_MAINTENANCE_VERSION, 0)

#endif // DATABASEVERSION_H
