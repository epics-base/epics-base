/* @(#)devAiSymb.c	1.1	6/4/93
 *      Device Support for VxWorks Global Symbol Analog Input Records
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
 *		The Control Systems Group
 *		Systems Engineering Department
 *		Lawrence Berkeley Laboratory
 *
 *      NOTES:
 * Derived from soft record device support.
 * Modification Log:
 * -----------------
 */


#include	<vxWorks.h>
#include 	<sysSymTbl.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<aiRecord.h>

/* Create the dset for devAiSymb */
long init_record();
long read_ai();
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
/*
    Looks for record name in global symbol table.  Strips off any 
    prefix separated by a ":" before the lookup, to allow same variable
    name in multiple ioc's.  Strips off suffixes separated by ";", to
    allow multiple references to same variable in an ioc.
*/
    struct aiRecord	*pai;
{
    char *nptr, pname[29];
    SYM_TYPE stype;

    /* variable names from c have a prepended underscore */
    strcpy(pname,"_");		/* in case it is unprefixed */
    strcat(pname, pai->name);
    nptr = strrchr(pname, ';');	/* find any suffix and */
    if (nptr)
       *nptr = '\0';		/* terminate it. */
    nptr = strchr(pname, ':');	/* find any prefix and */
    if (nptr)			/* bypass it */
       *nptr = '_';		/* overwrite : with _ */
    else
       nptr = pname;
    if (symFindByName(sysSymTbl, nptr, (char **)&pai->dpvt, &stype))
    {
	recGblRecordError(S_db_badField,(void *)pai,
	    "devAiSymb (init_record) Illegal NAME field");
	return(S_db_badField);
    }

    pai->linr=0;		/* prevent any conversions */
    return(0);
}

static long read_ai(pai)
    struct aiRecord	*pai;
{
    long status;

    if (pai->dpvt)
    {
       pai->val = *(double *)(pai->dpvt);
       status = 0;
    }
    else
       status = 1;
       
    if(RTN_SUCCESS(status)) pai->udf=FALSE;

    return(2); /*don't convert*/
}
