/* recPermissive.c */
/* base/src/rec  $Id$ */

/* recPermissive.c - Record Support Routines for Permissive records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            10-10-90
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
 * Modification Log:
 * -----------------
 * .01  10-10-90	mrk	extensible record and device support
 * .02  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .03  02-05-92	jba	Changed function arguments from paddr to precord 
 * .04  02-28-92	jba	ANSI C changes
 * .05  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
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
#include "errMdef.h"
#include "recSup.h"
#define GEN_SIZE_OFFSET
#include "permissiveRecord.h"
#undef  GEN_SIZE_OFFSET

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
#define init_record NULL
static long process();
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

struct rset permissiveRSET={
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
	get_alarm_double };

static void monitor();

static long process(ppermissive)
    struct permissiveRecord     *ppermissive;
{

    ppermissive->pact=TRUE;
    ppermissive->udf=FALSE;
    recGblGetTimeStamp(ppermissive);
    monitor(ppermissive);
    recGblFwdLink(ppermissive);
    ppermissive->pact=FALSE;
    return(0);
}

static void monitor(ppermissive)
    struct permissiveRecord             *ppermissive;
{
    unsigned short  monitor_mask;
    unsigned short  val,oval,wflg,oflg;

    monitor_mask = recGblResetAlarms(ppermissive);
    /* get val,oval,wflg,oflg*/
    val=ppermissive->val;
    oval=ppermissive->oval;
    wflg=ppermissive->wflg;
    oflg=ppermissive->oflg;
    /*set  oval and oflg*/
    ppermissive->oval = val;
    ppermissive->oflg = wflg;
    if(oval != val) {
	db_post_events(ppermissive,&ppermissive->val,
	    monitor_mask|DBE_VALUE|DBE_LOG);
    }
    if(oflg != wflg) {
        db_post_events(ppermissive,&ppermissive->wflg,
	    monitor_mask|DBE_VALUE|DBE_LOG);
    }
    return;
}
