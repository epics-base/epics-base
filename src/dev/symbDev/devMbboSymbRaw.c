/*************************************************************************\
* Copyright (c) 2002 Lawrence Berkeley Laboratory,
*     Systems Engineering Department, The Control Systems Group
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/dev $Id$ */

/* @(#)devMbboSymbRaw.c	1.1	6/4/93
 *      Device Support for VxWorks Global Symbol Raw Multi-bit Binary Output Records
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
#include        <sysSymTbl.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<module_types.h>
#include	<mbboRecord.h>
#include	<devSymb.h>

static long init_record();
static long write_mbbo();

/* Create the dset for devMbboSymbRaw */

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
}devMbboSymbRaw={
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
            "devMbboSymbRaw (init_record) Illegal NAME or OUT field");
        return(S_db_badField);
    }

    return(0);		
}

static long write_mbbo(pmbbo)
    struct mbboRecord	*pmbbo;
{
    struct vxSym *private = (struct vxSym *) pmbbo->dpvt;
    
    if (private)
       *((long *)(*private->ppvar) + private->index) = pmbbo->rval;
    else
       return(1);

    pmbbo->udf=FALSE;

    return(0);
}
