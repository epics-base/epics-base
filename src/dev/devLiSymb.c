/* base/src/dev $Id$ */

/* @(#)devLiSymb.c	1.1	6/4/93
 *      Device Support for VxWorks Global Symbol Longin Input Records
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
 */

#include	<vxWorks.h>
#include        <sysSymTbl.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<module_types.h>
#include	<longinRecord.h>

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
/*
    Looks for record name in global symbol table.  strips off any 
    prefix separated by a ":" before the lookup, to allow same variable
    name in multiple ioc's.  Strips off suffixes separated by ";", to
    allow multiple references to same variable in an ioc.
*/
    struct longinRecord	*plongin;
{
    char *nptr, pname[29];
    SYM_TYPE stype;

    /* variable names from c have a prepended underscore */
    strcpy(pname,"_");		/* in case it is unprefixed */
    strcat(pname, plongin->name);
    nptr = strrchr(pname, ';'); /* find any suffix and */
    if (nptr)
       *nptr = '\0';            /* terminate it. */
    nptr = strchr(pname, ':');	/* strip off prefix, if any */
    if (nptr)
       *nptr = '_';		/* overwrite : with _ */
    else
       nptr = pname;
    if (symFindByName(sysSymTbl, nptr, (char **)&plongin->dpvt, &stype))
    {
	recGblRecordError(S_db_badField,(void *)plongin,
	    "devLiSymb (init_record) Illegal NAME field");
	return(S_db_badField);
    }

    return(0);		
}

static long read_longin(plongin)
    struct longinRecord	*plongin;
{
    long status;

    if (plongin->dpvt)
    {
       plongin->val = *(long *)(plongin->dpvt);
       status = 0;
    }
    else
       status = 1;

    if(RTN_SUCCESS(status)) plongin->udf=FALSE;

    return(status);
}
