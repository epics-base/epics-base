/* recFanout.c */
/* share/src/rec $Id$ */

/* recFanout.c - Record Support Routines for Fanout records
 *
 * Author:      Bob Dalesio
 * Date:        12-20-88
 *
 *	Control System Software for the GTA Project
 *
 *	Copyright 1988, 1989, the Regents of the University of California.
 *
 *	This software was produced under a U.S. Government contract
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *	operated by the University of California for the U.S. Department
 *	of Energy.
 *
 *	Developed by the Controls and Automation Group (AT-8)
 *	Accelerator Technology Division
 *	Los Alamos National Laboratory
 *
 *	Direct inqueries to:
 *	Bob Dalesio, AT-8, Mail Stop H820
 *	Los Alamos National Laboratory
 *	Los Alamos, New Mexico 87545
 *	Phone: (505) 667-3414
 *	E-mail: dalesio@luke.lanl.gov
 *
 * Modification Log:
 * -----------------
 * .01  12-23-88        lrd     Alarm on locked MAX_LOCKED times
 * .02  05-03-89        lrd     removed process mask from arg list
 * .03  09-25-89        lrd     add conditional scanning
 * .04  01-21-90        lrd     unlock on scan disable exit
 * .05  04-19-90        lrd     user select disable on 0 or 1
 * .06  10-31-90	mrk	no user select disable on 0 or 1
 * .07  10-31-90	mrk	extensible record and device support
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<fanoutRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
#define init_record NULL
long process();
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
	get_alarm_double };

#define SELECT_ALL  0
#define SELECTED 1

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct fanoutRecord	*pfanout=(struct fanoutRecord *)(paddr->precord);
    short           stat,sevr,nsta,nsev;

    pfanout->pact = TRUE;
    if(pfanout->selm==SELECT_ALL) {
        if (pfanout->lnk1.type==DB_LINK) dbScanPassive(pfanout->lnk1.value.db_link.pdbAddr);
        if (pfanout->lnk2.type==DB_LINK) dbScanPassive(pfanout->lnk2.value.db_link.pdbAddr);
        if (pfanout->lnk3.type==DB_LINK) dbScanPassive(pfanout->lnk3.value.db_link.pdbAddr);
        if (pfanout->lnk4.type==DB_LINK) dbScanPassive(pfanout->lnk4.value.db_link.pdbAddr);
        if (pfanout->lnk5.type==DB_LINK) dbScanPassive(pfanout->lnk5.value.db_link.pdbAddr);
        if (pfanout->lnk6.type==DB_LINK) dbScanPassive(pfanout->lnk6.value.db_link.pdbAddr);
    } else if(pfanout->selm==SELECTED) {
	if(pfanout->seln<1 || pfanout->seln>6) {
	    if(pfanout->nsev<MAJOR_ALARM) {
		pfanout->nsev = MAJOR_ALARM;
		pfanout->nsta = SOFT_ALARM;
	    }
	} else {
	    struct link *plink;

	    plink=&(pfanout->lnk1);
	    plink += (pfanout->seln-1);
	    dbScanPassive(plink->value.db_link.pdbAddr);
	}
    } else {
	if(pfanout->nsev<MAJOR_ALARM) {
	    pfanout->nsev = MAJOR_ALARM;
	    pfanout->nsta = SOFT_ALARM;
	}
    }
    tsLocalTime(&pfanout->time);
    /* check monitors*/
    /* get previous stat and sevr  and new stat and sevr*/
    stat=pfanout->stat;
    sevr=pfanout->sevr;
    nsta=pfanout->nsta;
    nsev=pfanout->nsev;
    /*set current stat and sevr*/
    pfanout->stat = nsta;
    pfanout->sevr = nsev;
    pfanout->nsta = 0;
    pfanout->nsev = 0;
    if((stat!=nsta || sevr!=nsev)){
	db_post_events(pfanout,&pfanout->stat,DBE_VALUE);
	db_post_events(pfanout,&pfanout->sevr,DBE_VALUE);
    }
    pfanout->pact=FALSE;
    return(0);
}
