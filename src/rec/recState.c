/* recState.c */
/* share/src/rec $Id$ */

/* recState.c - Record Support Routines for State records
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
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<stateRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
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

struct rset stateRSET={
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

static long get_value(pstate,pvdes)
    struct stateRecord             *pstate;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_STRING;
    pvdes->no_elements=1;
    pvdes->pvalue = (caddr_t)(&pstate->val[0]);
    return(0);
}


static long process(paddr)
    struct dbAddr	*paddr;
{
	struct stateRecord	*pstate=(struct stateRecord *)(paddr->precord);

        pstate->pact=TRUE;
	if(strncmp(pstate->oval,pstate->val,sizeof(pstate->val))) {
        	if(pstate->mlis.count != 0)
                	db_post_events(pstate,&(pstate->val[0]),DBE_VALUE);
		strncpy(pstate->oval,pstate->val,sizeof(pstate->val));
	}
        /* process the forward scan link record */
        if (pstate->flnk.type==DB_LINK) dbScanPassive(pstate->flnk.value.db_link.pdbAddr);
        pstate->pact=FALSE;
	return(0);
}
