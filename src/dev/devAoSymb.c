/* @(#)devAoSymb.c	1.1	6/4/93
 *      Device Support for VxWorks Global Symbol Analog Output Records
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
#include	<sysSymTbl.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<special.h>
#include	<aoRecord.h>


/* Create the dset for devAoSymb */
long init_record();
long write_ao();
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
/*
    Looks for record name in global symbol table.  strips off any 
    prefix separated by a ":" before the lookup, to allow same variable
    name in multiple ioc's.  Strips off suffixes separated by ";", to
    allow multiple references to same variable in an ioc.
*/
    struct aoRecord	*pao;
{
    char *nptr, pname[29];
    SYM_TYPE stype;

    /* variable names from c have a prepended underscore */
    strcpy(pname,"_");		/* in case it is unprefixed */
    strcat(pname, pao->name);
    nptr = strrchr(pname, ';'); /* find any suffix and */
    if (nptr)
       *nptr = '\0';            /* terminate it. */
    nptr = strchr(pname, ':');	/* strip off prefix, if any */
    if (nptr)
       *nptr = '_';		/* overwrite : with _ */
    else
       nptr = pname;
    if (symFindByName(sysSymTbl, nptr, (char **)&pao->dpvt, &stype))
    {
	recGblRecordError(S_db_badField,(void *)pao,
	    "devAoSymb (init_record) Illegal NAME field");
	return(S_db_badField);
    }

    return(2);			/* don't convert */
}

static long write_ao(pao)
    struct aoRecord	*pao;
{
    if (pao->dpvt)
       *(double *)(pao->dpvt) = pao->val;
    else
       return(1);

    return(0);
}
