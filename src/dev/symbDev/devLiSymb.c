/*************************************************************************\
* Copyright (c) 2002 Lawrence Berkeley Laboratory,
*     Systems Engineering Department, The Control Systems Group
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/dev $Id$ */

/* @(#)devLiSymb.c	1.1	6/4/93
 *      Device Support for VxWorks Global Symbol Longin Input Records
 *
 *
 *      Author:         Carl Lionberger
 *      Date:           060893
 *
 *              The Control Systems Group
 *              Systems Engineering Department
 *              Lawrence Berkeley Laboratory
 *
 *      NOTES:
 * Derived from soft record device support.
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
#include	<longinRecord.h>
#include	<devSymb.h>

/* Create the dset for devLiSymb */
static long init_record();
static long read_longin();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_longin;
}devLiSymb={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_longin};

long testl = 5463;

static long init_record(plongin)
    struct longinRecord	*plongin;
{
    /* determine address of record value */
    if (devSymbFind(plongin->name, &plongin->inp, &plongin->dpvt))
    {
        recGblRecordError(S_db_badField,(void *)plongin,
            "devLiSymb (init_record) Illegal NAME or INP field");
        return(S_db_badField);
    }

    return(0);		
}

static long read_longin(plongin)
    struct longinRecord	*plongin;
{
    long status;
    struct vxSym *private = (struct vxSym *) plongin->dpvt;
	int lockKey;

    if (plongin->dpvt)
    {
	   lockKey = intLock();
       plongin->val = *((long *)(*private->ppvar) + private->index);
	   intUnlock(lockKey);
       status = 0;
    }
    else
       status = 1;

    if(RTN_SUCCESS(status)) plongin->udf=FALSE;

    return(status);
}
