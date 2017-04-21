/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* recStringin.c - Record Support Routines for Stringin records */
/*
 *      Author: 	Janet Anderson
 *      Date:   	4/23/91
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "stringinRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset stringinRSET={
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_units,
	get_precision,
	get_enum_str,
	get_enum_strs,
	put_enum_str,
	get_graphic_double,
	get_control_double,
	get_alarm_double
};
epicsExportAddress(rset,stringinRSET);

struct stringindset { /* stringin input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_stringin; /*returns: (-1,0)=>(failure,success)*/
};
static void monitor(stringinRecord *);
static long readValue(stringinRecord *);


static long init_record(struct dbCommon *pcommon, int pass)
{
    struct stringinRecord *prec = (struct stringinRecord *)pcommon;
    STATIC_ASSERT(sizeof(prec->oval)==sizeof(prec->val));
    struct stringindset *pdset = (struct stringindset *) prec->dset;

    if (pass==0)
        return 0;

    recGblInitConstantLink(&prec->siml, DBF_USHORT, &prec->simm);
    recGblInitConstantLink(&prec->siol, DBF_STRING, prec->sval);

    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "stringin: init_record");
        return S_dev_noDSET;
    }

    /* must have read_stringin function defined */
    if ((pdset->number < 5) || (pdset->read_stringin == NULL)) {
        recGblRecordError(S_dev_missingSup, prec, "stringin: init_record");
        return S_dev_missingSup;
    }

    if (pdset->init_record) {
        long status = pdset->init_record(prec);

        if (status)
            return status;
    }
    strcpy(prec->oval, prec->val);
    return 0;
}

/*
 */
static long process(struct dbCommon *pcommon)
{
    struct stringinRecord *prec = (struct stringinRecord *)pcommon;
    struct stringindset  *pdset = (struct stringindset *)(prec->dset);
	long		 status;
	unsigned char    pact=prec->pact;

	if( (pdset==NULL) || (pdset->read_stringin==NULL) ) {
		prec->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)prec,"read_stringin");
		return(S_dev_missingSup);
	}

	status=readValue(prec); /* read the new value */
	/* check if device support set pact */
	if ( !pact && prec->pact ) return(0);
	prec->pact = TRUE;

	recGblGetTimeStamp(prec);

	/* check event list */
	monitor(prec);
	/* process the forward scan link record */
	recGblFwdLink(prec);

	prec->pact=FALSE;
	return(status);
}

static void monitor(stringinRecord *prec)
{
    int monitor_mask = recGblResetAlarms(prec);

    if (strncmp(prec->oval, prec->val, sizeof(prec->val))) {
        monitor_mask |= DBE_VALUE | DBE_LOG;
        strncpy(prec->oval, prec->val, sizeof(prec->val));
    }

    if (prec->mpst == stringinPOST_Always)
        monitor_mask |= DBE_VALUE;
    if (prec->apst == stringinPOST_Always)
        monitor_mask |= DBE_LOG;

    if (monitor_mask)
        db_post_events(prec, prec->val, monitor_mask);
}

static long readValue(stringinRecord *prec)
{
	long		status;
        struct stringindset 	*pdset = (struct stringindset *) (prec->dset);

	if (prec->pact == TRUE){
		status=(*pdset->read_stringin)(prec);
		return(status);
	}

	status=dbGetLink(&(prec->siml),DBR_USHORT, &(prec->simm),0,0);
	if (status)
		return(status);

	if (prec->simm == menuYesNoNO){
		status=(*pdset->read_stringin)(prec);
		return(status);
	}
	if (prec->simm == menuYesNoYES){
		status=dbGetLink(&(prec->siol),DBR_STRING,
			prec->sval,0,0);
		if (status==0) {
			strcpy(prec->val,prec->sval);
			prec->udf=FALSE;
		}
	} else {
		status=-1;
		recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(prec,SIMM_ALARM,prec->sims);

	return(status);
}
