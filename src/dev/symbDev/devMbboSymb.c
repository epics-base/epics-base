/* base/src/dev $Id$ */

/* @(#)devMbboSymb.c	1.1	6/4/93
 *      Device Support for VxWorks Global Symbol Multi-bit Binary Output Records
 *
 *
 *      Author:         Carl Lionberger
 *      Date:           060893
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
 *              The Control Systems Group
 *              Systems Engineering Department
 *              Lawrence Berkeley Laboratory
 *
 *      NOTES:
 * Derived from soft record device support.
 * Modification Log:
 * -----------------
 * wfl  06-Jun-96       call devSymbFind() to parse PARM field and
 *                      look up symbol
 * anj  14-Oct-96	Changed devSymbFind() parameters.
 * wfl  13-Dec-96	created from bo support
 */


#include	<vxWorks.h>
#include	<sysSymTbl.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>
#include	<intLib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<module_types.h>
#include	<mbboRecord.h>
#include	<devSymb.h>

static long init_record();
static long write_mbbo();

/* Create the dset for devMbboSymb */

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
}devMbboSymb={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_mbbo};
 

static long init_record(pmbbo)
    struct mbboRecord	*pmbbo;
{
    /* determine address of record value */
    if (devSymbFind(pmbbo->name, &pmbbo->out, &pmbbo->dpvt))
    {
        recGblRecordError(S_db_badField,(void *)pmbbo,
            "devMbboSymb (init_record) Illegal NAME or OUT field");
        return(S_db_badField);
    }

    return(0);		
}

static long write_mbbo(pmbbo)
    struct mbboRecord	*pmbbo;
{
    int lockKey;
    struct vxSym *private = (struct vxSym *) pmbbo->dpvt;
    
    if (private)
    {
        lockKey = intLock();
        *((long *)(*private->ppvar) + private->index) = pmbbo->val;
        intUnlock(lockKey);
    }
    else
        return(1);

    pmbbo->udf=FALSE;

    return(0);
}
