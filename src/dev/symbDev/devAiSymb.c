/*************************************************************************\
* Copyright (c) 2002 Lawrence Berkeley Laboratory,
*     Systems Engineering Department, The Control Systems Group
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/dev $Id$ */

/* @(#)devAiSymb.c	1.1	6/4/93
 *      Device Support for VxWorks Global Symbol Analog Input Records
 *
 *
 *      Author:         Carl Lionberger
 *      Date:           060893
 *
 *		The Control Systems Group
 *		Systems Engineering Department
 *		Lawrence Berkeley Laboratory
 *
 *      NOTES:
 * Derived from soft record device support.
 */


#include	<vxWorks.h>
#include 	<sysSymTbl.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>
#include	<intLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<aiRecord.h>
#include	<devSymb.h>

/* Create the dset for devAiSymb */
static long init_record();
static long read_ai();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
}devAiSymb={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_ai,
	NULL};

double testd = 5.5;

static long init_record(pai)
    struct aiRecord	*pai;
{
    /* determine address of record value */
    if (devSymbFind(pai->name, &pai->inp, &pai->dpvt))
    {
	recGblRecordError(S_db_badField,(void *)pai,
	    "devAiSymb (init_record) Illegal NAME or INP field");
	return(S_db_badField);
    }

    pai->linr=0;		/* prevent any conversions */
    return(0);
}

static long read_ai(pai)
    struct aiRecord	*pai;
{
    long status;
    struct vxSym *private = (struct vxSym *) pai->dpvt;
	int lockKey;

    if (private)
    {
	   lockKey = intLock();
       pai->val = *((double *)(*private->ppvar) + private->index);
	   intUnlock(lockKey);
       status = 0;
    }
    else
       status = 1;
       
    if(RTN_SUCCESS(status)) pai->udf=FALSE;

    return(2); /*don't convert*/
}
