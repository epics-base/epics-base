/*************************************************************************\
* Copyright (c) 2002 Lawrence Berkeley Laboratory,
*     Systems Engineering Department, The Control Systems Group
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/dev $Id$ */

/* @(#)devBiSymb.c	1.1	6/4/93
 *      Device Support for VxWorks Global Symbol Multi-bit Binary Input Records
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
#include	<mbbiRecord.h>
#include	<devSymb.h>

/* Create the dset for devMbbiSymb */
static long init_record();
static long read_mbbi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;
}devMbbiSymb={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_mbbi};

long testmb = 1;

static long init_record(pmbbi)
    struct mbbiRecord	*pmbbi;
{
    /* determine address of record value */
    if (devSymbFind(pmbbi->name, &pmbbi->inp, &pmbbi->dpvt))
    {
        recGblRecordError(S_db_badField,(void *)pmbbi,
            "devMbbiSymb (init_record) Illegal NAME or INP field");
        return(S_db_badField);
    }

    return(0);		
}

static long read_mbbi(pmbbi)
    struct mbbiRecord	*pmbbi;
{
    int lockKey;
    long status;
    struct vxSym *private = (struct vxSym *) pmbbi->dpvt;

    if (pmbbi->dpvt)
    {
        lockKey = intLock();
        pmbbi->val = *((long *)(*private->ppvar) + private->index);
        intUnlock(lockKey);
        status = 0;
    }
    else
        status = 1;

    if(RTN_SUCCESS(status)) pmbbi->udf=FALSE;

    return(2); /*don't convert*/
}
