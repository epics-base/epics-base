/* devMbboDirectSoftRaw.c */
/* base/src/dev $Id$ */
/*
 *      Author:		Janet Anderson
 *      Current Author: Matthew Needes
 *      Date:		10-08-93
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 *    (log for devMbboSoftRaw.c applies)
 * .01  10-08-93  mcn   (created)     device support for devMbboDirect records
 */
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	"alarm.h"
#include	"dbDefs.h"
#include	"dbAccess.h"
#include	"recGbl.h"
#include        "recSup.h"
#include	"devSup.h"
#include	"mbboDirectRecord.h"

/* Create the dset for devMbboDirectSoftRaw */
static long init_record();
static long write_mbbo();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
}devMbboDirectSoftRaw={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_mbbo
};

static long init_record(pmbbo)
struct mbboDirectRecord *pmbbo;
{
    long status = 0;
 
    if (pmbbo->out.type != PV_LINK)
       status = 2;
    /*to preserve old functionality*/
    if(pmbbo->nobt == 0) pmbbo->mask = 0xffffffff;
    pmbbo->mask <<= pmbbo->shft;
    return status;
} /* end init_record() */

static long write_mbbo(pmbbo)
    struct mbboDirectRecord	*pmbbo;
{
    long status;
    unsigned long data;

    data = pmbbo->rval & pmbbo->mask;
    status = dbPutLink(&pmbbo->out,DBR_LONG, &data,1);
    return(0);
}
