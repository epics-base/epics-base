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
long report();
#define initialize NULL
#define init_record NULL
long process();
#define special NULL
#define get_precision NULL
long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_enum_str NULL
#define get_units NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_enum_strs NULL

struct rset permissiveRSET={
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_precision,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_enum_str,
	get_units,
	get_graphic_double,
	get_control_double,
	get_enum_strs };

static long report(fp,paddr)
    FILE	  *fp;
    struct dbAddr *paddr;
{
    struct permissiveRecord	*ppermissive=(struct permissiveRecord*)(paddr->precord);

    if(recGblReportDbCommon(fp,paddr)) return(-1);
    if(fprintf(fp,"LABL %s\n",ppermissive->labl)) return(-1);
    if(fprintf(fp,"VAL  %d\n",ppermissive->val)) return(-1);
    if(fprintf(fp,"WFLG %d\n",ppermissive->wflg)) return(-1);
    return(0);
}

static long get_value(ppermissive,pvdes)
    struct permissiveRecord             *ppermissive;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_SHORT;
    pvdes->no_elements=1;
    (short *)(pvdes->pvalue) = &(ppermissive->val);
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
	struct permissiveRecord	*ppermissive=(struct permissiveRecord *)(paddr->precord);

	ppermissive->pact=TRUE;
	if(ppermissive->mlis.count!=0)
		db_post_events(ppermissive,&ppermissive->val,DBE_VALUE);
	ppermissive->pact=FALSE;
	return(0);
}
