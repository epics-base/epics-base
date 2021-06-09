/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbLoadTemplate.h */

#ifndef INCdbLoadTemplateh
#define INCdbLoadTemplateh

#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

DBCORE_API int dbLoadTemplate(
    const char *sub_file, const char *cmd_collect);

#ifdef __cplusplus
}
#endif

#endif /*INCdbLoadTemplateh*/
