/*************************************************************************\
* Copyright (c) 2018 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 * Author: Andrew Johnson <anj@aps.anl.gov>
 */

#include <dbAccessDefs.h>
#include <recSup.h>

#define GEN_SIZE_OFFSET
#include "ioRecord.h"
#undef GEN_SIZE_OFFSET

#include <epicsExport.h>

static rset ioRSET;
epicsExportAddress(rset,ioRSET);
