/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *      Original Author: Bob Dalesio
 *      Date:            12-20-88
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
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "dbCommon.h"

#define GEN_SIZE_OFFSET
#include "fanoutRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(fanoutRecord *, int);
static long process(fanoutRecord *);
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

rset fanoutRSET={
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
epicsExportAddress(rset,fanoutRSET);

static long init_record(fanoutRecord *prec, int pass)
{

    if (pass==0) return(0);
    recGblInitConstantLink(&prec->sell,DBF_USHORT,&prec->seln);
    return(0);
}
	
static long process(fanoutRecord *prec)
{
    struct link    *plink;
    unsigned short state;
    short          i;
    unsigned short monitor_mask;

    prec->pact = TRUE;

    /* fetch link selection  */
    dbGetLink(&(prec->sell),DBR_USHORT,&(prec->seln),0,0);
    switch (prec->selm){
    case (fanoutSELM_All):
        plink=&(prec->lnk1);
        state=prec->seln;
        for ( i=0; i<6; i++, state>>=1, plink++) {
            if(plink->type!=CONSTANT) dbScanFwdLink(plink);
        }
        break;
    case (fanoutSELM_Specified):
        if(prec->seln>6) {
		recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
            break;
        }
        if(prec->seln==0) {
            break;
        }
        plink=&(prec->lnk1);
        plink += (prec->seln-1); dbScanFwdLink(plink);
        break;
    case (fanoutSELM_Mask):
        if(prec->seln==0) {
            break;
        }
        if(prec->seln>63 ) {
            recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
            break;
        }
        plink=&(prec->lnk1);
        state=prec->seln;
        for ( i=0; i<6; i++, state>>=1, plink++) {
            if(state & 1 && plink->type!=CONSTANT) dbScanFwdLink(plink);
        }
        break;
    default:
        recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
    }
    prec->udf=FALSE;
    recGblGetTimeStamp(prec);
    /* check monitors*/
    /* get previous stat and sevr  and new stat and sevr*/
    monitor_mask = recGblResetAlarms(prec);
    /* process the forward scan link record */
    recGblFwdLink(prec);
    prec->pact=FALSE;
    return(0);
}
