/* devMbbiDirectMpv910.c */
/* base/src/dev $Id$ */

/* devMbbiDirectMpv910.c - Device Support Routines*/
/* Burr Brown MPV 910  Multibit Binary input */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Matthew Needes
 *      Date:            10-08-93
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
 *   (modification log of devMbbiDirectMpv910 applies)
 *  .01  10-08-93     mcn  (created)  device support for direct mbbi
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
#include	<mbbiDirectRecord.h>


/* Create the dset for devMbbiDirectMpv910 */
static long init_record();
static long read_mbbi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;
}devMbbiDirectMpv910={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_mbbi};

static long init_record(pmbbi)
    struct mbbiDirectRecord	*pmbbi;
{

    /* mbbi.inp must be an VME_IO */
    switch (pmbbi->inp.type) {
    case (VME_IO) :
	pmbbi->shft = pmbbi->inp.value.vmeio.signal;
	pmbbi->mask <<= pmbbi->shft;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbi,
		"devMbbiDirectMpv910 (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_mbbi(pmbbi)
    struct mbbiDirectRecord	*pmbbi;
{
	struct vmeio	*pvmeio;
	int		status;
	unsigned long	value;

	
	pvmeio = (struct vmeio *)&(pmbbi->inp.value);
	status = bb910_driver(pvmeio->card,pmbbi->mask,&value);
	if(status==0) {
		pmbbi->rval = value;
	} else {
                recGblSetSevr(pmbbi,READ_ALARM,INVALID_ALARM);
	}
	return(status);
}
