/*************************************************************************\
* Copyright (c) 2002 Lawrence Berkeley Laboratory,
*     Systems Engineering Department, The Control Systems Group
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/dev $Id$ */

/* @(#)devAoSymb.c	1.1	6/4/93
 *      Device Support for VxWorks Global Symbol Analog Output Records
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
#include	<link.h>
#include	<special.h>
#include	<aoRecord.h>
#include	<devSymb.h>


/* Create the dset for devAoSymb */
static long init_record();
static long write_ao();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}devAoSymb={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_ao,
	NULL};


static long init_record(pao)
    struct aoRecord	*pao;
{
    /* determine address of record value */
    if (devSymbFind(pao->name, &pao->out, &pao->dpvt))
    {
        recGblRecordError(S_db_badField,(void *)pao,
            "devAoSymb (init_record) Illegal NAME or OUT field");
        return(S_db_badField);
    }

    return(2);			/* don't convert */
}

static long write_ao(pao)
    struct aoRecord	*pao;
{
    struct vxSym *private = (struct vxSym *) pao->dpvt;
	int lockKey;

    if (private)
    {
	   lockKey = intLock();
       *((double *)(*private->ppvar) + private->index) = pao->val;
	   intUnlock(lockKey);
    }
    else
       return(1);

    return(0);
}
