/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * devAaoSoft.c - Device Support Routines for soft Waveform Records
 * 
 *      Original Author: Bob Dalesio
 *      Current Author:  Dirk Zimoch
 *      Date:            27-MAY-2010
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "devSup.h"
#include "cantProceed.h"
#include "menuYesNo.h"
#include "aaoRecord.h"
#include "epicsExport.h"

/* Create the dset for devAaoSoft */
static long init_record();
static long write_aao();

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_aao;
} devAaoSoft = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    write_aao
};
epicsExportAddress(dset,devAaoSoft);

static long init_record(aaoRecord *prec)
{
    if (dbLinkIsConstant(&prec->out)) {
        prec->nord = 0;
    }
    return 0;
}

static long write_aao(aaoRecord *prec)
{
    long nRequest = prec->nord;
    dbPutLink(prec->simm == menuYesNoYES ? &prec->siol : &prec->out,
        prec->ftvl, prec->bptr, nRequest);

    return 0;
}
