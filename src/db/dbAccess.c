/* dbAccess.c */
/* base/src/db  $Id$ */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            11-7-90
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
 * .01  07-26-91	mrk	Allow choices to be retrieved as numeric
 * .02  08-13-91	mrk	Support db_field_log for dbGetField
 * .03  09-30-91	mrk	Support for TPRO and DISP
 * .04  10-03-91	jba	Bug fix in putStringUchar
 * .05  11-06-91	jba	Moved processing message before record process
 * .06  11-26-91	jba	Added return to dbGetLink
 *              		Fixed bug in special processing of SPC_MOD (100) 
 * .07  12-02-91	jba	Writing to PROC will always force record process
 * .08  02-05-92	jba	Changed function arguments from paddr to precord 
 * .09  03-02-92	jba	Added function dbValueSize to replace db_value_size
 * .10  04-17-92	rcz	put in mrk's dbNameToAddr changes for dbBase
 * .11  05-18-92	mrk	Changes for database internal structures
 * .12  07-16-92	jba	Added disable alarm severity, ansi c changes
 * .13  08-05-92	jba	Removed all references to dbr_field_type
 * .14  09-18-92	jba	replaced get of disa code with recGblGetLinkValue call
 * .15  07-15-93	mrk	Changes for new dbStaticLib
 * .16  02-02-94	mrk	Added dbPutNotify and caching
 * .17  02-02-94	mrk	Added init code for tsel
 * .18  03-18-94	mcn	Split dbAccess.c into dbAccess.c and dbLink.c
 * .19  03-18-94	mcn	Added breakpoint capability
 * .20  03-18-94	mcn	Added comments/whitespace
 * .21  10-20-95	mrk	Combined dbAccess and dbLink; Created dbConvert
 */


#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<string.h>
#include	<taskLib.h>
#include	<vxLib.h>
#include	<tickLib.h>

#include	<fast_lock.h>
#include	<cvtFast.h>
#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbBase.h>
#include	<dbAccess.h>
#include	<dbStaticLib.h>
#include	<dbConvert.h>
#include	<dbBkpt.h>
#include	<dbScan.h>
#include	<dbCommon.h>
#include	<dbLock.h>
#include	<dbFldTypes.h>
#include	<dbEvent.h>
#include	<db_field_log.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<recGbl.h>
#include	<special.h>
#include	<asLib.h>

extern struct dbBase *pdbbase;
extern long lset_stack_not_empty;

static short mapDBFToDBR[DBF_NTYPES] = {
	DBR_STRING, DBR_CHAR, DBR_UCHAR, DBR_SHORT, DBR_USHORT, 
	DBR_LONG, DBR_ULONG, DBR_FLOAT, DBR_DOUBLE,
	DBR_ENUM, DBR_ENUM, DBR_ENUM,
	DBR_STRING, DBR_STRING, DBR_STRING, DBR_NOACCESS
};

/*
 *  The number of consecutive attempts that can be made to process an
 *    active record before a SCAN_ALARM is raised.  Active records
 *    (records with the pact flag set) cannot be processed until
 *    that flag becomes unset.
 */
#define MAX_LOCK 10

static long putSpecial(DBADDR *paddr,int pass)
{
    long int	(*pspecial)()=NULL;
    struct rset	*prset;
    dbCommon 	*precord=(dbCommon *)(paddr->precord);
    long	status=0;
    long	special=paddr->special;
	
    prset = dbGetRset(paddr);
    if(special<100) { /*global processing*/
	if((special==SPC_NOMOD) && (pass==0)) {
	    status = S_db_noMod;
	    recGblDbaddrError(status,paddr,"dbPut");
	    return(status);
	}else if(special==SPC_SCAN){
	    if(pass==0)
		scanDelete(precord);
	    else
		scanAdd(precord);
	}else if((special==SPC_AS) && (pass==1)) {
	    asChangeGroup((ASMEMBERPVT *)&precord->asp,precord->asg);
	}
    }else {
	if( prset && (pspecial = (prset->special))) {
	    status=(*pspecial)(paddr,pass);
	    if(status) return(status);
	} else if(pass==0){
	    recGblRecSupError(S_db_noSupport,paddr,"dbPut", "special");
	    return(S_db_noSupport);
	}
    }
    return(0);
}

static void get_enum_strs(DBADDR *paddr,void **ppbuffer,
	struct rset *prset,long	*options)
{
	short		field_type=paddr->field_type;
	dbFldDes	*pdbFldDes = (dbFldDes *)paddr->pfldDes;
	dbMenu		*pdbMenu;
	dbDeviceMenu	*pdbDeviceMenu;
	char		**papChoice;
	unsigned long	no_str;
	char		*ptemp;
	struct dbr_enumStrs *pdbr_enumStrs=(struct dbr_enumStrs*)(*ppbuffer);
	int			i;

	memset(pdbr_enumStrs,'\0',dbr_enumStrs_size);
	switch(field_type) {
		case DBF_ENUM:
		    if( prset && prset->get_enum_strs ) {
			(*prset->get_enum_strs)(paddr,pdbr_enumStrs);
		    } else {
			*options = (*options)^DBR_ENUM_STRS;/*Turn off option*/
		    }
		    break;
		case DBF_MENU:
		    pdbMenu = (dbMenu *)pdbFldDes->ftPvt;
		    no_str = pdbMenu->nChoice;
		    papChoice= pdbMenu->papChoiceValue;
		    goto choice_common;
		case DBF_DEVICE:
		    pdbDeviceMenu = (dbDeviceMenu *)pdbFldDes->ftPvt;
		    no_str = pdbDeviceMenu->nChoice;
		    papChoice = pdbDeviceMenu->papChoice;
		    goto choice_common;
choice_common:
		    i = sizeof(pdbr_enumStrs->strs)/
			sizeof(pdbr_enumStrs->strs[0]);
		    if(i<no_str) no_str = i;
		    pdbr_enumStrs->no_str = no_str;
		    ptemp = &(pdbr_enumStrs->strs[0][0]);
		    for (i=0; i<no_str; i++) {
			if(papChoice[i]==NULL) *ptemp=0;
			else {
			    strncpy(ptemp,papChoice[i],
				sizeof(pdbr_enumStrs->strs[0]));
			    *(ptemp+sizeof(pdbr_enumStrs->strs[0])-1) = 0;
			}
			ptemp += sizeof(pdbr_enumStrs->strs[0]);
		    }
		    break;
		default:
		    *options = (*options)^DBR_ENUM_STRS;/*Turn off option*/
		    break;
	}
	*ppbuffer = ((char *)*ppbuffer) + dbr_enumStrs_size;
	return;
}

static void get_graphics(DBADDR *paddr,void **ppbuffer,
	struct rset *prset,long	*options)
{
	struct			dbr_grDouble grd;
	int			got_data=FALSE;

	if( prset && prset->get_graphic_double ) {
		(*prset->get_graphic_double)(paddr,&grd);
		got_data=TRUE;
	}
	if( (*options) & (DBR_GR_LONG) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_grLong *pgr=(struct dbr_grLong*)pbuffer;
		    pgr->upper_disp_limit = grd.upper_disp_limit;
		    pgr->lower_disp_limit = grd.lower_disp_limit;
		} else {
		    memset(pbuffer,'\0',dbr_grLong_size);
		    *options = (*options) ^ DBR_GR_LONG; /*Turn off option*/
		}
		*ppbuffer = ((char *)*ppbuffer) + dbr_grLong_size;
	}
	if( (*options) & (DBR_GR_DOUBLE) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_grDouble *pgr=(struct dbr_grDouble*)pbuffer;
		    pgr->upper_disp_limit = grd.upper_disp_limit;
		    pgr->lower_disp_limit = grd.lower_disp_limit;
		} else {
		    memset(pbuffer,'\0',dbr_grDouble_size);
		    *options = (*options) ^ DBR_GR_DOUBLE; /*Turn off option*/
		}
		*ppbuffer = ((char *)*ppbuffer) + dbr_grDouble_size;
	}
	return;
}

static void get_control(DBADDR *paddr,void **ppbuffer,
	struct rset *prset,long	*options)
{
	struct dbr_ctrlDouble	ctrld;
	int			got_data=FALSE;

	if( prset && prset->get_control_double ) {
		(*prset->get_control_double)(paddr,&ctrld);
		got_data=TRUE;
	}
	if( (*options) & (DBR_CTRL_LONG) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_ctrlLong *pctrl=(struct dbr_ctrlLong*)pbuffer;
		    pctrl->upper_ctrl_limit = ctrld.upper_ctrl_limit;
		    pctrl->lower_ctrl_limit = ctrld.lower_ctrl_limit;
		} else {
		    memset(pbuffer,'\0',dbr_ctrlLong_size);
		    *options = (*options) ^ DBR_CTRL_LONG; /*Turn off option*/
		}
		*ppbuffer = ((char *)*ppbuffer) + dbr_ctrlLong_size;
	}
	if( (*options) & (DBR_CTRL_DOUBLE) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		   struct dbr_ctrlDouble *pctrl=(struct dbr_ctrlDouble*)pbuffer;
		   pctrl->upper_ctrl_limit = ctrld.upper_ctrl_limit;
		   pctrl->lower_ctrl_limit = ctrld.lower_ctrl_limit;
		} else {
		    memset(pbuffer,'\0',dbr_ctrlDouble_size);
		    *options = (*options) ^ DBR_CTRL_DOUBLE; /*Turn off option*/
		}
		*ppbuffer = ((char *)*ppbuffer) + dbr_ctrlDouble_size;
	}
	return;
}

static void get_alarm(DBADDR *paddr,void	**ppbuffer,
	struct rset *prset,long	*options)
{
	struct			dbr_alDouble ald;
	int			got_data=FALSE;

	if( prset && prset->get_alarm_double ) {
		(*prset->get_alarm_double)(paddr,&ald);
		got_data=TRUE;
	}
	if( (*options) & (DBR_AL_LONG) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_alLong *pal=(struct dbr_alLong*)pbuffer;
		    pal->upper_alarm_limit = ald.upper_alarm_limit;
		    pal->upper_warning_limit = ald.upper_warning_limit;
		    pal->lower_warning_limit = ald.lower_warning_limit;
		    pal->lower_alarm_limit = ald.lower_alarm_limit;
		} else {
		    memset(pbuffer,'\0',dbr_alLong_size);
		    *options = (*options) ^ DBR_AL_LONG; /*Turn off option*/
		}
		*ppbuffer = ((char *)*ppbuffer) + dbr_alLong_size;
	}
	if( (*options) & (DBR_AL_DOUBLE) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_alDouble *pal=(struct dbr_alDouble*)pbuffer;
		    pal->upper_alarm_limit = ald.upper_alarm_limit;
		    pal->upper_warning_limit = ald.upper_warning_limit;
		    pal->lower_warning_limit = ald.lower_warning_limit;
		    pal->lower_alarm_limit = ald.lower_alarm_limit;
		} else {
		    memset(pbuffer,'\0',dbr_alDouble_size);
		    *options = (*options) ^ DBR_AL_DOUBLE; /*Turn off option*/
		}
		*ppbuffer = ((char *)*ppbuffer) + dbr_alDouble_size;
	}
	return;
}

static void getOptions(DBADDR *paddr,void **poriginal,long *options,void *pflin)
{
	db_field_log	*pfl= (db_field_log *)pflin;
	struct rset	*prset;
	short		field_type=paddr->field_type;
	dbCommon	*pcommon;
	void		*pbuffer = *poriginal;


	prset=dbGetRset(paddr);
	/* Process options */
	pcommon = (dbCommon *)(paddr->precord);
	if( (*options) & DBR_STATUS ) {
	    if(pfl!=NULL) {
		/* This gives warning:
		 * ANSI C forbids use of cast expressions as lvalues
		 */
		*((unsigned short *)pbuffer)++ = pfl->stat;
		*((unsigned short *)pbuffer)++ = pfl->sevr;
	    } else {
		*((unsigned short *)pbuffer)++ = pcommon->stat;
		*((unsigned short *)pbuffer)++ = pcommon->sevr;
	    }
	    *((unsigned short *)pbuffer)++ = pcommon->acks;
	    *((unsigned short *)pbuffer)++ = pcommon->ackt;
	}
	if( (*options) & DBR_UNITS ) {
	    memset(pbuffer,'\0',dbr_units_size);
	    if( prset && prset->get_units ){ 
		char *	pchar = (char *)pbuffer;

		(*prset->get_units)(paddr,pchar);
		pchar[DB_UNITS_SIZE-1] = '\0';
	    } else {
		*options = (*options) ^ DBR_UNITS; /*Turn off DBR_UNITS*/
	    }
	    pbuffer = (char *)pbuffer + dbr_units_size;
	}
	if( (*options) & DBR_PRECISION ) {
	    struct dbr_precision *pdbr_precision=
		(struct dbr_precision *)pbuffer;

	    memset(pbuffer,'\0',dbr_precision_size);
	    if((field_type==DBF_FLOAT || field_type==DBF_DOUBLE)
	    &&  prset && prset->get_precision ){ 
		(*prset->get_precision)(paddr,pbuffer);
		if(pdbr_precision->field_width<=0)
			pdbr_precision->field_width =
				pdbr_precision->precision + 5;
	    } else {
		*options = (*options)^DBR_PRECISION; /*Turn off DBR_PRECISION*/
	    }
	    pbuffer = (char *)pbuffer + dbr_precision_size;
	}
	if( (*options) & DBR_TIME ) {
	    if(pfl!=NULL) {
		*((unsigned long *)pbuffer)++ = pfl->time.secPastEpoch;
		*((unsigned long *)pbuffer)++ = pfl->time.nsec;
	    } else {
		*((unsigned long *)pbuffer)++ = pcommon->time.secPastEpoch;
		*((unsigned long *)pbuffer)++ = pcommon->time.nsec;
	    }
	}
	if( (*options) & DBR_ENUM_STRS )
		get_enum_strs(paddr,&pbuffer,prset,options);
	if( (*options) & (DBR_GR_LONG|DBR_GR_DOUBLE ))
			get_graphics(paddr,&pbuffer,prset,options);
	if((*options) & (DBR_CTRL_LONG | DBR_CTRL_DOUBLE ))
			get_control(paddr,&pbuffer,prset,options);
	if((*options) & (DBR_AL_LONG | DBR_AL_DOUBLE ))
			get_alarm(paddr,&pbuffer,prset,options);
	*poriginal = pbuffer;
}

struct rset *dbGetRset(struct dbAddr *paddr)
{
	struct dbFldDes *pfldDes = (struct dbFldDes *)paddr->pfldDes;

	if(!pfldDes) return(0);
	return(pfldDes->pdbRecordType->prset);
}

int dbIsValueField(struct dbFldDes *pdbFldDes)
{
    if(pdbFldDes->pdbRecordType->indvalFlddes == pdbFldDes->indRecordType)
	return(TRUE);
    else
	return(FALSE);
}

int dbGetFieldIndex(struct dbAddr *paddr)
{
    return(((struct dbFldDes *)paddr->pfldDes)->indRecordType);
}

long dbGetNelements(struct link *plink,long *nelements)
{
    switch(plink->type) {
    case CONSTANT:
	*nelements = 0;
	return(0);
    case DB_LINK: {
	    DBADDR *paddr = (DBADDR *)plink->value.pv_link.pvt;
	    *nelements = paddr->no_elements;
	    return(0);
	}
    case CA_LINK:
	return(dbCaGetNelements(plink,nelements));
    default:
	break;
    }
    return(S_db_badField);
}

int dbIsLinkConnected(struct link *plink)
{
    switch(plink->type) {
	case DB_LINK: return(TRUE);
	case CA_LINK: return(dbCaIsLinkConnected(plink));
	default: break;
    }
    return(FALSE);
}

/*
 *  Process a record if its scan field is passive.
 *     Will notify if processing is complete by callback.
 *       (only if you are interested in completion)
 */
long dbScanPassive(dbCommon *pfrom, dbCommon *pto)
{
    long status;
	
    /* if not passive just return success */
    if(pto->scan != 0) return(0);

    if(pfrom && pfrom->ppn) dbNotifyAdd(pfrom,pto);
    status = dbProcess(pto);
    return(status);
}

/*KLUDGE: Following needed so that dbPutLink to PROC field works correctly*/
long dbScanLink(dbCommon *pfrom, dbCommon *pto)
{
    long		status;
    unsigned char	pact;

    if(pfrom && pfrom->ppn) dbNotifyAdd(pfrom,pto);
    pact = pfrom->pact;
    pfrom->pact = TRUE;
    status = dbProcess(pto);
    pfrom->pact = pact;
    return(status);
}

void dbScanFwdLink(struct link *plink)
{
    dbCommon		*precord;
    struct pv_link      *pvlink;
    short               fwdLinkValue;

    if(plink->type!=DB_LINK && plink->type!=CA_LINK) return;
    pvlink = &plink->value.pv_link;
    precord = pvlink->precord;
    if(plink->type==DB_LINK) {
        DBADDR *paddr = (DBADDR *)plink->value.pv_link.pvt;
        dbScanPassive(precord,paddr->precord);
        return;
    }
    if(!(pvlink->pvlMask & pvlOptFWD)) return;
    fwdLinkValue = 1;
    dbCaPutLink(plink,DBR_SHORT,&fwdLinkValue,1);
    return;
}

/*
 *   Process the record.
 *     1.  Check for breakpoints.
 *     2.  Check the process active flag (PACT).
 *     3.  Check the disable link.
 *     4.  Check the RSET (record support entry table) exists.
 *     5.  Run the process routine specific to the record type.
 *     6.  Check to see if record contents should be automatically printed.
 */
long dbProcess(dbCommon *precord)
{
	struct rset	*prset = (struct rset *)precord->rset;
	dbRecordType	*pdbRecordType = (dbRecordType *)precord->rdes;
	unsigned char	tpro=precord->tpro;
	unsigned long	lset;
	long		status = 0;
	static char 	trace=0;
	static unsigned long trace_lset=0;
	int		set_trace=FALSE;
	dbFldDes	*pdbFldDes;

	lset = dbLockGetLockId(precord);
        /*
         *  Note that it is likely that if any changes are made
         *   to dbProcess() corresponding changes will have to
         *   be made in the breakpoint handler.
         */
 
        /* see if there are any stopped records or breakpoints */
        if (lset_stack_not_empty) {
           /*
            *  Check to see if the record should be processed
            *   and activate breakpoint accordingly.  If this
            *   function call returns non-zero, skip record
            *   support and fall out of dbProcess().  This is
            *   done so that a dbContTask() can be spawned to
            *   take over record processing for the lock set
            *   containing a breakpoint.
            */
            if (dbBkpt(precord))
                goto all_done;
        }
    
	/* check for trace processing*/
	if (tpro) {
		if (vxTas(&trace)) {
			trace_lset = lset;
			set_trace = TRUE;
		}
	}

	/* If already active dont process */
	if (precord->pact) {
		unsigned short	monitor_mask;

		if (trace && trace_lset==lset)
			printf("active:    %s\n",precord->name);
		/* raise scan alarm after MAX_LOCK times */
		if (precord->stat==SCAN_ALARM) goto all_done;
		if (precord->lcnt++ !=MAX_LOCK) goto all_done;
		if (precord->sevr>=INVALID_ALARM) goto all_done;
		recGblSetSevr(precord, SCAN_ALARM, INVALID_ALARM);
		monitor_mask = recGblResetAlarms(precord);
		monitor_mask |= DBE_VALUE;
		pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->indvalFlddes];
		db_post_events(precord,
			(void *)(((char *)precord) + pdbFldDes->offset),
			monitor_mask);
		goto all_done;
	}
        else precord->lcnt = 0;

       /*
        *  Check the record disable link.  A record will not be
        *    processed if the value retrieved through this link
        *    is equal to constant set in the record's disv field.
        */
        status = dbGetLink(&(precord->sdis),DBR_SHORT,&(precord->disa),0,0);

	/* if disabled check disable alarm severity and return success */
	if (precord->disa == precord->disv) {
		if (trace && trace_lset==lset)
			printf("disabled:  %s\n",precord->name);

		/*take care of caching and notifyCompletion*/
		precord->rpro = FALSE;
		if (precord->ppn) dbNotifyCompletion(precord);
		/* raise disable alarm */
		if (precord->stat==DISABLE_ALARM) goto all_done;
		precord->sevr = precord->diss;
		precord->stat = DISABLE_ALARM;
		precord->nsev = 0;
		precord->nsta = 0;
		db_post_events(precord, &precord->stat, DBE_VALUE);
		db_post_events(precord, &precord->sevr, DBE_VALUE);
		pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->indvalFlddes];
		db_post_events(precord,
			(void *)(((char *)precord) + pdbFldDes->offset),
			DBE_VALUE|DBE_ALARM);
		goto all_done;
	}

	/* locate record processing routine */
        /* put this in iocInit() !!! */
	if (!(prset=(struct rset *)precord->rset) || !(prset->process)) {
		precord->pact=1;/*set pact TRUE so error is issued only once*/
		recGblRecordError(S_db_noRSET, (void *)precord, "dbProcess");
		status = S_db_noRSET;
		if (trace && trace_lset == lset)
			printf("failure:   %s\n", precord->name);

		goto all_done;
	}
	if (trace && trace_lset==lset) printf("process:   %s\n", precord->name);
	/* process record */
	status = (*prset->process)(precord);
        /* Print record's fields if PRINT_MASK set in breakpoint field */
        if (lset_stack_not_empty) {
                dbPrint(precord);
        }
all_done:
	if (set_trace) {
		trace_lset = 0;
		trace = 0;
	}
	return(status);
}

/*
 *  Fill out a database structure (*paddr) for
 *    a record given by the name "pname."
 *
 *    Returns error codes from StaticLib module, not
 *        from dbAccess.
 */
long dbNameToAddr(const char *pname,DBADDR *paddr)
{
	DBENTRY		dbEntry;
	long		status=0;
	struct rset	*prset;
	dbFldDes	*pflddes;

	dbInitEntry(pdbbase,&dbEntry);
	if(status = dbFindRecord(&dbEntry,pname)) return(status);
	paddr->precord = dbEntry.precnode->precord;
	if(!dbEntry.pfield) {
		if(status=dbFindField(&dbEntry,"VAL"))return(status);
	}
	paddr->pfield = dbEntry.pfield;
	pflddes = dbEntry.pflddes;
	paddr->pfldDes = (void *)pflddes;
	paddr->field_type = pflddes->field_type;
	paddr->dbr_field_type = mapDBFToDBR[pflddes->field_type];
	paddr->field_size = pflddes->size;
	paddr->special = pflddes->special;

	/*if special is SPC_DBADDR then call cvt_dbaddr		*/
	/*it may change pfield,no_elements,field_type,dbr_field_type,*/
	/*field_size,and special*/
	paddr->no_elements=1;
	if(((paddr->special)==SPC_DBADDR)
	&& (prset=dbGetRset(paddr))
	&& (prset->cvt_dbaddr)) status = (*prset->cvt_dbaddr)(paddr);
	return(status);
}

long dbValueSize(
	short     dbr_type
)
{
     /* sizes for value associated with each DBR request type */
     static long size[] = {
        MAX_STRING_SIZE,                /* STRING       */
        sizeof(char),                   /* CHAR         */
        sizeof(unsigned char),          /* UCHAR        */
        sizeof(short),                  /* SHORT        */
        sizeof(unsigned short),         /* USHORT       */
        sizeof(long),                   /* LONG         */
        sizeof(unsigned long),          /* ULONG        */
        sizeof(float),                  /* FLOAT        */
        sizeof(double),                 /* DOUBLE       */
        sizeof(unsigned short)};        /* ENUM         */

     return(size[dbr_type]);
}


long dbBufferSize(
     short     dbr_type,
     long      options,
     long      no_elements
)
{
    long nbytes=0;

    nbytes += dbValueSize(dbr_type) * no_elements;
    if(options & DBR_STATUS)	nbytes += dbr_status_size;
    if(options & DBR_UNITS)	nbytes += dbr_units_size;
    if(options & DBR_PRECISION) nbytes += dbr_precision_size;
    if(options & DBR_TIME)	nbytes += dbr_time_size;
    if(options & DBR_ENUM_STRS)	nbytes += dbr_enumStrs_size;
    if(options & DBR_GR_LONG)	nbytes += dbr_grLong_size;
    if(options & DBR_GR_DOUBLE)	nbytes += dbr_grDouble_size;
    if(options & DBR_CTRL_LONG) nbytes += dbr_ctrlLong_size;
    if(options & DBR_CTRL_DOUBLE)nbytes += dbr_ctrlDouble_size;
    if(options & DBR_AL_LONG)   nbytes += dbr_alLong_size;
    if(options & DBR_AL_DOUBLE) nbytes += dbr_alDouble_size;
    return(nbytes);
}

long dbGetLinkValue(struct link	*plink, short dbrType, void *pbuffer,
	long *poptions, long *pnRequest)
{
    long		status = 0;

    if(plink->type==CONSTANT) {
	if(poptions) *poptions = 0;
	if(pnRequest) *pnRequest = 0;
    } else if(plink->type==DB_LINK) {
	dbCommon	*precord = plink->value.pv_link.precord;
	struct pv_link	*ppv_link = &(plink->value.pv_link);
	DBADDR		*paddr = ppv_link->pvt;

	/* scan passive records with links that are process passive  */
	if(ppv_link->pvlMask&pvlOptPP) {
	    dbCommon	*pfrom = paddr->precord;
	    unsigned char   pact;

	    pact = precord->pact;
	    precord->pact = TRUE;
	    status = dbScanPassive(precord,pfrom);
	    precord->pact = pact;
	    if(status) return(status);
	}
	if(ppv_link->pvlMask&pvlOptMS && precord!= paddr->precord)
	    recGblSetSevr(precord,LINK_ALARM,paddr->precord->sevr);

	if(ppv_link->getCvt && ppv_link->lastGetdbrType==dbrType) {
	    status = (*ppv_link->getCvt)(paddr->pfield,pbuffer, paddr);
	} else {
	    unsigned short	dbfType= paddr->field_type;
	    long		no_elements = paddr->no_elements;

	    if((dbrType<0) || (dbrType>DBR_ENUM) || (dbfType > DBF_DEVICE)) {
		status = S_db_badDbrtype;
		recGblRecordError(status,(void *)precord,"GetLinkValue Failed");
		recGblSetSevr(precord,LINK_ALARM,INVALID_ALARM);
		return(status);
	    }
	    /*  attempt to make a fast link */
	    if((!poptions || (*poptions == 0))
	    && (no_elements == 1)
	    && (!pnRequest || (*pnRequest == 1))) {
		ppv_link->getCvt = dbFastGetConvertRoutine[dbfType][dbrType];
		status = (*ppv_link->getCvt) (paddr->pfield,pbuffer, paddr);
	    }else{
		status=dbGet(paddr,dbrType,pbuffer,poptions,pnRequest,NULL);
		ppv_link->getCvt = 0;
	    }
	}
	ppv_link->lastGetdbrType = dbrType;
	if(status){
	    recGblRecordError(status,(void *)precord,"dbGetLinkValue");
	    recGblSetSevr(precord,LINK_ALARM,INVALID_ALARM);
	}
    }else if(plink->type==CA_LINK) {
	struct dbCommon	*precord = plink->value.pv_link.precord;
	struct pv_link	*pcalink = &(plink->value.pv_link);
	unsigned short sevr;

	status=dbCaGetLink(plink,dbrType,pbuffer,&sevr,pnRequest);
	if(status) {
	    recGblSetSevr(precord,LINK_ALARM,INVALID_ALARM);
	}else if(pcalink->pvlMask&pvlOptMS){
	    recGblSetSevr(precord,LINK_ALARM,sevr);
	}
	if(poptions) *poptions = 0;
    } else {
	status = -1;
	errMessage(-1,"dbGetLinkValue: Illegal link type");
	taskSuspend(0);
    }
    return(status);
}

long dbPutLinkValue(struct link *plink,short dbrType,
	const void *pbuffer,long nRequest)
{
	long		status=0;

	if(plink->type==DB_LINK) {
		struct dbCommon	*psource = plink->value.pv_link.precord;
		struct pv_link	*ppv_link= &(plink->value.pv_link);
		DBADDR		*paddr	= (DBADDR*)(ppv_link->pvt);
		dbCommon 	*pdest	= paddr->precord;

		status=dbPut(paddr,dbrType,pbuffer,nRequest);
		if(ppv_link->pvlMask&pvlOptMS)
			recGblSetSevr(pdest,LINK_ALARM,psource->sevr);
		if(status) return(status);
		if((paddr->pfield==(void *)&pdest->proc)
		|| (ppv_link->pvlMask&pvlOptPP && pdest->scan==0)) {
			/*Note:If ppn then dbNotifyCancel will reprocess*/
			/*if dbPutField caused asyn record to process   */
			/* ask for reprocessing*/
			if(!psource->ppn && pdest->putf) pdest->rpro = TRUE;
			/* otherwise ask for the record to be processed*/
			else status=dbScanLink(psource,pdest);
		}
		if(status) recGblSetSevr(psource,LINK_ALARM,INVALID_ALARM);
	} else if(plink->type==CA_LINK) {
		struct dbCommon	*psource = plink->value.pv_link.precord;

		status = dbCaPutLink(plink,dbrType,pbuffer, nRequest);
		if(status < 0)
			recGblSetSevr(psource,LINK_ALARM,INVALID_ALARM);
	} else {
		status=-1;
		errMessage(-1,"dbPutLinkValue: Illegal link type");
		taskSuspend(0);
	}
	return(status);
}

long dbGetField( DBADDR	*paddr,short dbrType,void *pbuffer,
	long *options,long *nRequest,void *pflin)
{
	dbCommon	*precord = (dbCommon *)(paddr->precord);
	long		status = 0;
	short		dbfType = paddr->field_type;

	dbScanLock(precord);
	if(dbfType>=DBF_INLINK && dbfType<=DBF_FWDLINK) {
		DBENTRY	dbEntry;
		dbFldDes *pfldDes = (dbFldDes *)paddr->pfldDes;
		char	*rtnString;
		char	*pbuf = (char *)pbuffer;

		if(dbrType!=DBR_STRING) {
			status = S_db_badDbrtype;
			goto done;
		}
		if(options && (*options))
			getOptions(paddr,(void **)&pbuf,options,pflin);
		if(nRequest && *nRequest==0) goto done;
		dbInitEntry(pdbbase,&dbEntry);
		if(status = dbFindRecord(&dbEntry,precord->name)) goto done;
		if(status = dbFindField(&dbEntry,pfldDes->name)) goto done;
		rtnString = dbGetString(&dbEntry);
		/*begin kludge for old db_access MAX_STRING_SIZE*/
		if(strlen(rtnString)>=MAX_STRING_SIZE) {
			strncpy(pbuf,rtnString,MAX_STRING_SIZE-1);
			pbuf[MAX_STRING_SIZE-1] = 0;
		} else {
		    strcpy(pbuf,rtnString);
		}
		/*end kludge for old db_access MAX_STRING_SIZE*/
		dbFinishEntry(&dbEntry);
		goto done;
			
	} else {
		status = dbGet(paddr,dbrType,pbuffer,options,nRequest,pflin);
	}
done:
	dbScanUnlock(precord);
	return(status);
}

long dbGet(DBADDR *paddr,short dbrType,void *pbuffer,long *options,
	long *nRequest,void *pflin)
{
	db_field_log	*pfl= (db_field_log *)pflin;
	long		no_elements=paddr->no_elements;
	long 		offset;
	struct rset	*prset;
	short		field_type=paddr->field_type;
	long		(*pconvert_routine)();
	long		status = 0;
	char		message[80];


	prset=dbGetRset(paddr);
	if(options && (*options)) {
		void *pbuf = pbuffer;

		getOptions(paddr,&pbuf,options,pflin);
		pbuffer = pbuf;
	}
        if(nRequest && *nRequest==0) return(0);
	/* Check for valid request */
	if( INVALID_DB_REQ(dbrType) || (field_type>DBF_DEVICE) ){
		sprintf(message,"dbGet - database request type is %d",dbrType);
		recGblDbaddrError(S_db_badDbrtype,paddr,message);
		return(S_db_badDbrtype);
	}
	/* check for array			*/
	if( no_elements>1 && prset && (prset->get_array_info) ) {
		status = (*prset->get_array_info)(paddr,&no_elements,&offset);
	}
	else offset=0;
	if(offset==0 && (!nRequest || no_elements==1)) {
		if(nRequest) *nRequest = 1;
		if(pfl!=NULL) {
		    DBADDR	localAddr;

		    localAddr = *paddr; /*Structure copy*/
		    localAddr.pfield = (char *)&pfl->field;
		    status = (*dbFastGetConvertRoutine[field_type][dbrType])
				(localAddr.pfield,pbuffer, &localAddr);
		} else {
		    status = (*dbFastGetConvertRoutine[field_type][dbrType])
				(paddr->pfield,pbuffer, paddr);
		}
	} else {
		long	n;

		if(nRequest) {
			if(no_elements<(*nRequest)) *nRequest = no_elements;
			n = *nRequest;
		} else {
			n = 1;
		}
		pconvert_routine=dbGetConvertRoutine[field_type][dbrType];
		if(!pconvert_routine) {
			sprintf(message,"dbGet - database request type is %d",
				dbrType);
			recGblDbaddrError(S_db_badDbrtype,paddr,message);
			return(S_db_badDbrtype);
		}
		/* convert database field  and place it in the buffer */
		if(n<=0) {
			;/*do nothing*/
		} else if(pfl!=NULL) {
		    DBADDR	localAddr;
	
		    localAddr = *paddr; /*Structure copy*/
		    localAddr.pfield = (char *)&pfl->field;
		    status=(*pconvert_routine)(&localAddr,pbuffer,n,
				no_elements,offset);
		} else {
		    status=(*pconvert_routine)(paddr,pbuffer,n,
				no_elements,offset);
		}
	}
        return(status);
}

long dbPutField(DBADDR *paddr,short dbrType,const void *pbuffer,long  nRequest)
{
	long		status = 0;
	long		special=paddr->special;
	dbFldDes	*pfldDes=(dbFldDes *)(paddr->pfldDes);
	dbCommon 	*precord = (dbCommon *)(paddr->precord);
	short		dbfType = paddr->field_type;

	/*check for putField disabled*/
	if(precord->disp) {
		if((void *)(&precord->disp) != paddr->pfield) return(0);
	}
	if(dbfType>=DBF_INLINK && dbfType<=DBF_FWDLINK) {
		DBLINK  	*plink = (DBLINK *)paddr->pfield;
		DBENTRY		dbEntry;
		dbFldDes 	*pfldDes = (dbFldDes *)paddr->pfldDes;
		char		buffer[MAX_STRING_SIZE+2];
		int		len,j;
		char		*lastblank;

		if(dbrType!=DBR_STRING) return(S_db_badDbrtype);
		/*begin kludge for old db_access MAX_STRING_SIZE*/
		/*Allow M for MS and (N or NM) for NMS */
		strcpy(buffer,(char *)pbuffer);
		/*Strip trailing blanks*/
		len = strlen(buffer);
		for(j=len-1; j>0; j--) {
		    if(buffer[j]==' ')
			buffer[j] = 0;
		    else
			break;
		}
		lastblank = strrchr(buffer,' ');
		if(lastblank) {
		    if(strcmp(lastblank,"M")==0) {
			strcpy(lastblank,"MS");
		    } else {
			if((strcmp(lastblank,"N")==0)
			|| (strcmp(lastblank,"NM")==0)) {
			    strcpy(lastblank,"NMS");
			}
		    }
		}
		/*End kludge for old db_access MAX_STRING_SIZE*/
		dbLockSetGblLock();
		dbLockSetRecordLock(precord);
		if((plink->type == DB_LINK)||(plink->type == CA_LINK)) {
		    if(plink->type == DB_LINK) {
			free(plink->value.pv_link.pvt);
			plink->value.pv_link.pvt = 0;
			plink->type = PV_LINK;
			dbLockSetSplit(precord);
		    } else if(plink->type == CA_LINK) {
			dbCaRemoveLink(plink);
		    }
		    plink->value.pv_link.getCvt = 0;
		    plink->value.pv_link.pvlMask = 0;
		    plink->value.pv_link.lastGetdbrType = 0;
		    plink->type = PV_LINK;
		}
		dbInitEntry(pdbbase,&dbEntry);
		if(status=dbFindRecord(&dbEntry,precord->name)) goto done;
		if(status=dbFindField(&dbEntry,pfldDes->name))  goto done;
		/* check for special processing	is required */
		if(special) {
		    status = putSpecial(paddr,0);
		    if(status) return(status);
		}
		if(status=dbPutString(&dbEntry,buffer)) goto done;
		if(special) {
		    status = putSpecial(paddr,1);
		    if(status) return(status);
		}
		if(plink->type == PV_LINK) {
			DBADDR		dbaddr;

			if(!(plink->value.pv_link.pvlMask
			     &(pvlOptCA|pvlOptCP|pvlOptCPP))
			&&(dbNameToAddr(plink->value.pv_link.pvname,&dbaddr)==0)){
				DBADDR	*pdbAddr;

				plink->type = DB_LINK;
				pdbAddr = dbCalloc(1,sizeof(struct dbAddr));
				*pdbAddr = dbaddr; /*structure copy*/;
				plink->value.pv_link.precord = precord;
				plink->value.pv_link.pvt = pdbAddr;
				dbLockSetMerge(precord,pdbAddr->precord);
			} else {/*It is a CA link*/
			    char	*pperiod;

			    plink->type = CA_LINK;
			    plink->value.pv_link.precord = precord;
			    if(pfldDes->field_type==DBF_INLINK) {
				plink->value.pv_link.pvlMask |= pvlOptInpNative;
			    }
			    dbCaAddLink(plink);
			    if(pfldDes->field_type==DBF_FWDLINK) {
				pperiod = strrchr(plink->value.pv_link.pvname,
					'.');
				if(pperiod && strstr(pperiod,"PROC"))
				    plink->value.pv_link.pvlMask |= pvlOptFWD;
			    }
			}
		}
    		db_post_events(precord,plink,DBE_VALUE);
		dbFinishEntry(&dbEntry);
done:
		dbLockSetGblUnlock();
		return(status);
	}
	dbScanLock(precord);
	status=dbPut(paddr,dbrType,pbuffer,nRequest);
	if(status==0){
        	if((paddr->pfield==(void *)&precord->proc)
		||(pfldDes->process_passive && precord->scan==0 
		&& dbrType<DBR_PUT_ACKT)) {
		    if(precord->pact) {
			if(precord->tpro)
			    printf("active:    %s\n",precord->name);
			precord->rpro = TRUE;
		    } else {
			/*indicate that dbPutField called dbProcess*/
			precord->putf = TRUE;
			status=dbProcess(precord);
		    }
		}
	}
	dbScanUnlock(precord);
	return(status);
}

static long putAckt(paddr,pbuffer,nRequest,no_elements,offset)
DBADDR	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    dbCommon *precord=(dbCommon *)(paddr->precord);

    if(*pbuffer == precord->ackt) return(0);
    precord->ackt = *pbuffer;
    db_post_events(precord,&precord->ackt,DBE_VALUE|DBE_ALARM);
    if(!precord->ackt && precord->acks>precord->sevr) {
	precord->acks = precord->sevr;
	db_post_events(precord,&precord->acks,DBE_VALUE|DBE_ALARM);
    }
    db_post_events(precord,NULL,DBE_ALARM);
    return(0);
}

static long putAcks(paddr,pbuffer,nRequest,no_elements,offset)
DBADDR	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    dbCommon *precord=(dbCommon *)(paddr->precord);

    if(*pbuffer >= precord->acks) {
	precord->acks = 0;
	db_post_events(precord,NULL,DBE_ALARM);
	db_post_events(precord,&precord->acks,DBE_VALUE|DBE_ALARM);
    }
    return(0);
}

long dbPut(DBADDR *paddr,short dbrType,const void *pbuffer,long nRequest)
{
	long		no_elements=paddr->no_elements;
	long		dummy;
	long		offset;
	struct rset	*prset;
	dbCommon 	*precord=(dbCommon *)(paddr->precord);
	long		status=0;
	dbFldDes	*pfldDes;
	long		special=paddr->special;
	short		field_type=paddr->field_type;
	int		isValueField;
	char		message[80];

	if(dbrType==DBR_PUT_ACKT && field_type<=DBF_DEVICE) {
	    status=putAckt(paddr,pbuffer,(long)1,(long)1,(long)0);
	    return(status);
	} else if(dbrType==DBR_PUT_ACKS && field_type<=DBF_DEVICE) {
	    status=putAcks(paddr,pbuffer,(long)1,(long)1,(long)0);
	    return(status);
	} else if( INVALID_DB_REQ(dbrType) || (field_type>DBF_DEVICE)) {
		sprintf(message,"dbPut - database request type is %d",dbrType);
		recGblDbaddrError(S_db_badDbrtype,paddr,message);
		return(S_db_badDbrtype);
	}
	
	prset=dbGetRset(paddr);
	
	/* check for special processing	is required */
	if(special) {
	    status = putSpecial(paddr,0);
	    if(status) return(status);
	}
	if(no_elements<=1) {
		status = (*dbFastPutConvertRoutine[dbrType][field_type])
			(pbuffer,paddr->pfield, paddr);
	} else {
		if(prset && (prset->get_array_info) ) {
			status= (*prset->get_array_info)
				(paddr,&dummy,&offset);
		}
		else offset=0;
		if(no_elements<(nRequest)) nRequest = no_elements;
		status=(*dbPutConvertRoutine[dbrType][field_type])
			(paddr,pbuffer,nRequest,no_elements,offset);
		/* update array info	*/
		if(prset && (prset->put_array_info) ) {
			status= (*prset->put_array_info)
				(paddr,nRequest);
		}
	}
	if(status) return(status);

	/* check for special processing	is required */
	if(special) {
	    status = putSpecial(paddr,1);
	    if(status) return(status);
	}
	/* propagate events for this field */
	/* if the field is VAL and pvlMask&pvlOptPP is true dont propagate*/
	pfldDes = (dbFldDes *)(paddr->pfldDes);
	isValueField = dbIsValueField(pfldDes);
	if (isValueField) precord->udf=FALSE;
	if(precord->mlis.count &&
	(!isValueField || (!pfldDes->process_passive)))
		db_post_events(precord,paddr->pfield,DBE_VALUE);

	return(status);
}
