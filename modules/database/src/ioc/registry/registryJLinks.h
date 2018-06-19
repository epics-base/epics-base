/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef INC_registryJLinks_H
#define INC_registryJLinks_H

#include "dbBase.h"
#include "dbJLink.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int registryJLinkAdd(DBBASE *pbase, jlif *pjlif);

#ifdef __cplusplus
}
#endif


#endif /* INC_registryDriverSupport_H */
