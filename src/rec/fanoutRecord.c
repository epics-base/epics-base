/* fanoutRecord.c */
/* base/src/rec  $Id$ */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            12-20-88
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
 * .01  12-23-88        lrd     Alarm on locked MAX_LOCKED times
 * .02  05-03-89        lrd     removed process mask from arg list
 * .03  09-25-89        lrd     add conditional scanning
 * .04  01-21-90        lrd     unlock on scan disable exit
 * .05  04-19-90        lrd     user select disable on 0 or 1
 * .06  10-31-90        mrk        no user select disable on 0 or 1
 * .07  10-31-90        mrk        extensible record and device support
 * .08  09-25-91        jba     added input link for seln
 * .09  09-26-91        jba     added select_mask option
 * .10  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .11  02-05-92	jba	Changed function arguments from paddr to precord 
 * .12  02-05-92	jba	Added FWD scan link 
 * .13  02-28-92	jba	ANSI C changes
 * .14  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .15  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .16  09-10-92        jba     replaced fetch link selection with call to recGblGetLinkValue
 * .17	03-29-94	mcn	converted to fast links
 */

#include        <vxWorks.h>
#include        <types.h>
#include        <stdioLib.h>
#include        <lstLib.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include        <alarm.h>
#include        <dbAccess.h>
#include        <dbEvent.h>
#include        <dbFldTypes.h>
#include        <errMdef.h>
#include        <recSup.h>
#define GEN_SIZE_OFFSET
#include        <fanoutRecord.h>
#undef  GEN_SIZE_OFFSET
#include        <dbCommon.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
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

struct rset fanoutRSET={
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

static long init_record(pfanout,pass)
    struct fanoutRecord	*pfanout;
    int pass;
{

    if (pass==0) return(0);
    recGblInitConstantLink(&pfanout->sell,DBF_USHORT,&pfanout->seln);
    return(0);
}
	
static long process(pfanout)
    struct fanoutRecord     *pfanout;
{
    struct link    *plink;
    unsigned short state;
    short          i;
    unsigned short monitor_mask;

    pfanout->pact = TRUE;

    /* fetch link selection  */
    dbGetLink(&(pfanout->sell),DBR_USHORT,&(pfanout->seln),0,0);
    switch (pfanout->selm){
    case (fanoutSELM_All):
        plink=&(pfanout->lnk1);
        state=pfanout->seln;
        for ( i=0; i<6; i++, state>>=1, plink++) {
            if(plink->type!=CONSTANT) dbScanFwdLink(plink);
        }
        break;
    case (fanoutSELM_Specified):
        if(pfanout->seln>6) {
		recGblSetSevr(pfanout,SOFT_ALARM,INVALID_ALARM);
            break;
        }
        if(pfanout->seln==0) {
            break;
        }
        plink=&(pfanout->lnk1);
        plink += (pfanout->seln-1); dbScanFwdLink(plink);
        break;
    case (fanoutSELM_Mask):
        if(pfanout->seln==0) {
            break;
        }
        if(pfanout->seln>63 ) {
            recGblSetSevr(pfanout,SOFT_ALARM,INVALID_ALARM);
            break;
        }
        plink=&(pfanout->lnk1);
        state=pfanout->seln;
        for ( i=0; i<6; i++, state>>=1, plink++) {
            if(state & 1 && plink->type!=CONSTANT) dbScanFwdLink(plink);
        }
        break;
    default:
        recGblSetSevr(pfanout,SOFT_ALARM,INVALID_ALARM);
    }
    pfanout->udf=FALSE;
    recGblGetTimeStamp(pfanout);
    /* check monitors*/
    /* get previous stat and sevr  and new stat and sevr*/
    monitor_mask = recGblResetAlarms(pfanout);
    /* process the forward scan link record */
    recGblFwdLink(pfanout);
    pfanout->pact=FALSE;
    return(0);
}
