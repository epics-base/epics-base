/*************************************************************************\
* Copyright (c) 2002 Lawrence Berkeley Laboratory,
*     Systems Engineering Department, The Control Systems Group
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/dev $Id$ */

/* @(#)devLoSymb.c	1.1	6/4/93
 *      Device Support for VxWorks Global Symbol Longout Output Records
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
#include	<longoutRecord.h>
#include	<devSymb.h>

static long init_record();
static long write_longout();

/* Create the dset for devLoSymb */

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_longout;
}devLoSymb={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_longout};
 

static long init_record(plongout)
    struct longoutRecord	*plongout;
{
    /* determine address of record value */
    if (devSymbFind(plongout->name, &plongout->out, &plongout->dpvt))
    {
        recGblRecordError(S_db_badField,(void *)plongout,
            "devLoSymb (init_record) Illegal NAME or OUT field");
        return(S_db_badField);
    }

    return(0);		
}

static long write_longout(plongout)
    struct longoutRecord	*plongout;
{
    struct vxSym *private = (struct vxSym *) plongout->dpvt;
	int lockKey;

    if (private)
	{
	   lockKey = intLock();
       *((long *)(*private->ppvar) + private->index) = plongout->val;
	   intUnlock(lockKey);
	}
    else
       return(1);

    plongout->udf=FALSE;

    return(0);
}
