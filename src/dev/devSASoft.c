/* devSASoft.c */
/* devSASoft.c - Device Support Routines for soft subArray Records
 *
 *
 *      Author:         Carl Lionberger
 *      Date:           090293
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
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<subArrayRecord.h>

static int sizeofTypes[] = {MAX_STRING_SIZE,1,1,2,2,4,4,4,8,2};

/* Create the dset for devSASoft */
static long init_record();
static long read_sa();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_sa;
}devSASoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_sa};


static long init_record(psa)
    struct subArrayRecord	*psa;
{
    long status;

    /* sa.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (psa->inp.type) {
    case (CONSTANT) :
	psa->nord = 0;
	break;
    case (PV_LINK) :
    case (DB_LINK) :
    case (CA_LINK) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)psa,
		"devSASoft (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_sa(psa)
    struct subArrayRecord	*psa;
{
    long status,ecount,nRequest;

    if ((psa->indx + psa->nelm) < psa->malm)
       nRequest= psa->indx + psa->nelm;
    else
       nRequest=psa->malm;
    status = dbGetLink(&psa->inp,psa->ftvl,psa->bptr, 0,&nRequest);
    if ((nRequest - psa->indx) > 0)
    {
       if (psa->nelm > (nRequest - psa->indx))
          ecount = nRequest - psa->indx;
       else
          ecount = psa->nelm;
       bcopy(psa->bptr + psa->indx * sizeofTypes[psa->ftvl], psa->bptr, 
             ecount * sizeofTypes[psa->ftvl]);
    }
    psa->nord = nRequest - psa->indx;

    return(0);
}
