/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

/* recState.c - Record Support Routines for State records */
/*
 *      Original Author: Bob Dalesio
 *      Date:            10-10-90 
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#define GEN_SIZE_OFFSET
#include "stateRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
#define init_record NULL
static long process(stateRecord *);
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

rset stateRSET={
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
epicsExportAddress(rset,stateRSET);

static void monitor(stateRecord *);

static long process(stateRecord *pstate)
{

	pstate->udf = FALSE;
        pstate->pact=TRUE;
	recGblGetTimeStamp(pstate);
	monitor(pstate);
        /* process the forward scan link record */
        recGblFwdLink(pstate);
        pstate->pact=FALSE;
	return(0);
}

static void monitor(stateRecord *pstate)
{
    unsigned short  monitor_mask;

    /* get previous stat and sevr  and new stat and sevr*/
    monitor_mask = recGblResetAlarms(pstate);
    if(strncmp(pstate->oval,pstate->val,sizeof(pstate->val))) {
        db_post_events(pstate,&(pstate->val[0]),monitor_mask|DBE_VALUE|DBE_LOG);
	strncpy(pstate->oval,pstate->val,sizeof(pstate->val));
    }
    return;
}
