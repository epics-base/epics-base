/*************************************************************************\
* Copyright (c) 2002 Lawrence Berkeley Laboratory,The Control Systems
*     Group, Systems Engineering Department
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devSASoft.c */
/* devSASoft.c - Device Support Routines for soft subArray Records
 *
 *      Author:         Carl Lionberger
 *      Date:           090293
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "subArrayRecord.h"
#include "epicsExport.h"

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
	read_sa
};
epicsExportAddress(dset,devSASoft);


static long init_record(subArrayRecord *psa)
{

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

static long read_sa(subArrayRecord *psa)
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
       memcpy(psa->bptr,(char *)psa->bptr + psa->indx * sizeofTypes[psa->ftvl],
             ecount * sizeofTypes[psa->ftvl]);
    }
    psa->nord = nRequest - psa->indx;

    return(0);
}
