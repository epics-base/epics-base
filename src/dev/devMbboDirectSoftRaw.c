/* devMbboDirectSoftRaw.c */
/* base/src/dev $Id$ */

/* devMbboDirectSoftRaw.c - Device Support SoftRaw Direct mbbo */
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


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<module_types.h>
#include	<mbboDirectRecord.h>

static long init_record();

/* Create the dset for devMbboDirectSoftRaw */
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
	write_mbbo};



static long init_record(pmbbo)
struct mbboDirectRecord *pmbbo;
{
    long status;
 
    if (pmbbo->out.type != PV_LINK)
       status = 2;
    return status;
} /* end init_record() */

static long write_mbbo(pmbbo)
    struct mbboDirectRecord	*pmbbo;
{
    long status;

    status = dbPutLink(&pmbbo->out,DBR_LONG, &pmbbo->rval,1);
    return(0);
}
