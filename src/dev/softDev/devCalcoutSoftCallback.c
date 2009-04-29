/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devCalcoutSoftCallback.c */

/*
 *      Author:  Marty Kraimer
 *      Date:    05DEC2003
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "special.h"
#include "postfix.h"
#include "calcoutRecord.h"
#include "epicsExport.h"

static long write_calcout(calcoutRecord *prec);

struct {
    long	number;
    DEVSUPFUN	report;
    DEVSUPFUN	init;
    DEVSUPFUN	init_record;
    DEVSUPFUN	get_ioint_info;
    DEVSUPFUN	write;
} devCalcoutSoftCallback = {
    5, NULL, NULL, NULL, NULL, write_calcout
};
epicsExportAddress(dset, devCalcoutSoftCallback);

static long write_calcout(calcoutRecord *prec)
{
    struct link *plink = &prec->out;
    long status;

    if (prec->pact) return 0;
    if (plink->type != CA_LINK) {
        status = dbPutLink(plink, DBR_DOUBLE, &prec->oval, 1);
        return status;
    }
    status = dbCaPutLinkCallback(plink, DBR_DOUBLE, &prec->oval, 1,
        dbCaCallbackProcess, plink);
    if (status) {
        recGblSetSevr(prec, LINK_ALARM, INVALID_ALARM);
        return status;
    }
    prec->pact = TRUE;
    return 0;
}
