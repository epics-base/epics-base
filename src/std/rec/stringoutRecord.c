/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* recStringout.c - Record Support Routines for Stringout records */
/*
 * Author: 	Janet Anderson
 * Date:	4/23/91
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
#include "menuOmsl.h"
#include "menuIvoa.h"
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "stringoutRecord.h"
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

rset stringoutRSET={
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
epicsExportAddress(rset,stringoutRSET);

struct stringoutdset { /* stringout input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_stringout;/*(-1,0)=>(failure,success)*/
};
static void monitor(stringoutRecord *);
static long writeValue(stringoutRecord *);


static long init_record(struct dbCommon *pcommon, int pass)
{
    struct stringoutRecord *prec = (struct stringoutRecord *)pcommon;
    STATIC_ASSERT(sizeof(prec->oval)==sizeof(prec->val));
    struct stringoutdset *pdset = (struct stringoutdset *) prec->dset;

    if (pass==0)
        return 0;

    recGblInitConstantLink(&prec->siml, DBF_USHORT, &prec->simm);

    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "stringout: init_record");
        return S_dev_noDSET;
    }

    /* must have  write_stringout functions defined */
    if ((pdset->number < 5) || (pdset->write_stringout == NULL)) {
        recGblRecordError(S_dev_missingSup, prec, "stringout: init_record");
        return S_dev_missingSup;
    }

    /* get the initial value dol is a constant*/
    if (recGblInitConstantLink(&prec->dol, DBF_STRING, prec->val))
        prec->udf = FALSE;

    if (pdset->init_record) {
        long status = pdset->init_record(prec);

        if(status)
            return status;
    }

    strcpy(prec->oval, prec->val);
    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct stringoutRecord *prec = (struct stringoutRecord *)pcommon;
    struct stringoutdset  *pdset = (struct stringoutdset *)(prec->dset);
	long		 status=0;
	unsigned char    pact=prec->pact;

	if( (pdset==NULL) || (pdset->write_stringout==NULL) ) {
		prec->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)prec,"write_stringout");
		return(S_dev_missingSup);
	}
        if (!prec->pact &&
            !dbLinkIsConstant(&prec->dol) &&
            prec->omsl == menuOmslclosed_loop) {
                status = dbGetLink(&prec->dol, DBR_STRING, prec->val, 0, 0);
                if (!dbLinkIsConstant(&prec->dol) && !status)
                    prec->udf=FALSE;
        }

        if(prec->udf == TRUE ){
                recGblSetSevr(prec,UDF_ALARM,prec->udfs);
        }

        if (prec->nsev < INVALID_ALARM )
                status=writeValue(prec); /* write the new value */
        else {
                switch (prec->ivoa) {
                    case (menuIvoaContinue_normally) :
                        status=writeValue(prec); /* write the new value */
                        break;
                    case (menuIvoaDon_t_drive_outputs) :
                        break;
                    case (menuIvoaSet_output_to_IVOV) :
                        if(prec->pact == FALSE){
                                strcpy(prec->val,prec->ivov);
                        }
                        status=writeValue(prec); /* write the new value */
                        break;
                    default :
                        status=-1;
                        recGblRecordError(S_db_badField,(void *)prec,
                                "stringout:process Illegal IVOA field");
                }
        }

	/* check if device support set pact */
	if ( !pact && prec->pact ) return(0);

	prec->pact = TRUE;
	recGblGetTimeStamp(prec);
	monitor(prec);
	recGblFwdLink(prec);
	prec->pact=FALSE;
	return(status);
}

static void monitor(stringoutRecord *prec)
{
    int monitor_mask = recGblResetAlarms(prec);

    if (strncmp(prec->oval, prec->val, sizeof(prec->val))) {
        monitor_mask |= DBE_VALUE | DBE_LOG;
        strncpy(prec->oval, prec->val, sizeof(prec->val));
    }

    if (prec->mpst == stringoutPOST_Always)
        monitor_mask |= DBE_VALUE;
    if (prec->apst == stringoutPOST_Always)
        monitor_mask |= DBE_LOG;

    if (monitor_mask)
        db_post_events(prec, prec->val, monitor_mask);
}

static long writeValue(stringoutRecord *prec)
{
	long		status;
        struct stringoutdset 	*pdset = (struct stringoutdset *) (prec->dset);

	if (prec->pact == TRUE){
		status=(*pdset->write_stringout)(prec);
		return(status);
	}

	status=dbGetLink(&(prec->siml),DBR_USHORT,
		&(prec->simm),0,0);
	if (status)
		return(status);

	if (prec->simm == menuYesNoNO){
		status=(*pdset->write_stringout)(prec);
		return(status);
	}
	if (prec->simm == menuYesNoYES){
		status=dbPutLink(&prec->siol,DBR_STRING,
			prec->val,1);
	} else {
		status=-1;
		recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(prec,SIMM_ALARM,prec->sims);

	return(status);
}
