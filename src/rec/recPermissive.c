/* recPermissive.c */
/* share/src/rec $Id$ */

/* recPermissive.c - Record Support Routines for Permissive records
 *
 * Author: 	Marty Kraimer
 * Date:	10/10/90
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
 * .01  10-10-90	mrk	extensible record and device support
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<permissiveRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
#define init_record NULL
long process();
#define special NULL
long get_value();
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

void monitor();

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct permissiveRecord    *ppermissive=(struct permissiveRecord *)(paddr->precord);

    ppermissive->pact=TRUE;
    monitor(ppermissive);
    if (ppermissive->flnk.type==DB_LINK)
        dbScanPassive(ppermissive->flnk.value.db_link.pdbAddr);
    ppermissive->pact=FALSE;
    return(0);
}


static long get_value(ppermissive,pvdes)
    struct permissiveRecord             *ppermissive;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_USHORT;
    pvdes->no_elements=1;
    (unsigned short *)(pvdes->pvalue) = &(ppermissive->val);
    return(0);
}

static void monitor(ppermissive)
    struct permissiveRecord             *ppermissive;
{
    unsigned short  monitor_mask;
    short           stat,sevr,nsta,nsev;
    unsigned short  val,oval,wflg,oflg;

    /* get previous stat and sevr  and new stat and sevr*/
    stat=ppermissive->stat;
    sevr=ppermissive->sevr;
    nsta=ppermissive->nsta;
    nsev=ppermissive->nsev;
    /*set current stat and sevr*/
    ppermissive->stat = nsta;
    ppermissive->sevr = nsev;
    ppermissive->nsta = 0;
    ppermissive->nsev = 0;
    /* get val,oval,wflg,oflg*/
    val=ppermissive->val;
    oval=ppermissive->oval;
    wflg=ppermissive->wflg;
    oflg=ppermissive->oflg;
    /*set  oval and oflg*/
    ppermissive->oval = val;
    ppermissive->oflg = wflg;

    monitor_mask = 0;

    /* alarm condition changed this scan */
    if (stat!=nsta || sevr!=nsev) {
            /* post events for alarm condition change*/
            monitor_mask = DBE_ALARM;
            /* post stat and nsev fields */
            db_post_events(ppermissive,&ppermissive->stat,DBE_VALUE);
            db_post_events(ppermissive,&ppermissive->sevr,DBE_VALUE);
    }

    if(oval != val) {
	db_post_events(ppermissive,&ppermissive->val,monitor_mask|DBE_VALUE);
    }
    if(oflg != wflg) {
        db_post_events(ppermissive,&ppermissive->wflg,monitor_mask|DBE_VALUE);
    }
    return;
}
