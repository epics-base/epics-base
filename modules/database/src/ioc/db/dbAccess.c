/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbAccess.c */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *                       Andrew Johnson <anj@aps.anl.gov>
 *                       Ralph Lange <Ralph.Lange@bessy.de>
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "cantProceed.h"
#include "cvtFast.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "epicsMath.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "errlog.h"
#include "errMdef.h"

#define epicsExportSharedSymbols
#include "caeventmask.h"
#include "callback.h"
#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbBkpt.h"
#include "dbCommonPvt.h"
#include "dbConvertFast.h"
#include "dbConvert.h"
#include "dbEvent.h"
#include "db_field_log.h"
#include "dbFldTypes.h"
#include "dbFldTypes.h"
#include "dbLink.h"
#include "dbLockPvt.h"
#include "dbNotify.h"
#include "dbScan.h"
#include "dbServer.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "devSup.h"
#include "epicsEvent.h"
#include "link.h"
#include "recGbl.h"
#include "recSup.h"
#include "special.h"

epicsShareDef struct dbBase *pdbbase = 0;
epicsShareDef volatile int interruptAccept=FALSE;

/* Hook Routines */

epicsShareDef DB_LOAD_RECORDS_HOOK_ROUTINE dbLoadRecordsHook = NULL;

static short mapDBFToDBR[DBF_NTYPES] = {
    /* DBF_STRING   => */    DBR_STRING,
    /* DBF_CHAR     => */    DBR_CHAR,
    /* DBF_UCHAR    => */    DBR_UCHAR,
    /* DBF_SHORT    => */    DBR_SHORT,
    /* DBF_USHORT   => */    DBR_USHORT,
    /* DBF_LONG     => */    DBR_LONG,
    /* DBF_ULONG    => */    DBR_ULONG,
    /* DBF_INT64    => */    DBR_INT64,
    /* DBF_UINT64   => */    DBR_UINT64,
    /* DBF_FLOAT    => */    DBR_FLOAT,
    /* DBF_DOUBLE   => */    DBR_DOUBLE,
    /* DBF_ENUM,    => */    DBR_ENUM,
    /* DBF_MENU,    => */    DBR_ENUM,
    /* DBF_DEVICE   => */    DBR_ENUM,
    /* DBF_INLINK   => */    DBR_STRING,
    /* DBF_OUTLINK  => */    DBR_STRING,
    /* DBF_FWDLINK  => */    DBR_STRING,
    /* DBF_NOACCESS => */    DBR_NOACCESS
};

/*
 *  The number of consecutive attempts that can be made to process an
 *    active record before a SCAN_ALARM is raised.  Active records
 *    (records with the pact flag set) cannot be processed until
 *    that flag becomes unset.
 */
#define MAX_LOCK 10

/* The following is to handle SPC_AS */
static SPC_ASCALLBACK spcAsCallback = 0;

void dbSpcAsRegisterCallback(SPC_ASCALLBACK func)
{
    spcAsCallback = func;
}

long dbPutSpecial(DBADDR *paddr,int pass)
{
    long int	(*pspecial)()=NULL;
    rset        *prset;
    dbCommon 	*precord = paddr->precord;
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
            if(spcAsCallback) (*spcAsCallback)(precord);
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

static void get_enum_strs(DBADDR *paddr, char **ppbuffer,
    rset *prset,long	*options)
{
	short		field_type=paddr->field_type;
	dbFldDes	*pdbFldDes = paddr->pfldDes;
	dbMenu		*pdbMenu;
	dbDeviceMenu	*pdbDeviceMenu;
	char		**papChoice;
	unsigned long	no_str;
	char		*ptemp;
	struct dbr_enumStrs *pdbr_enumStrs=(struct dbr_enumStrs*)(*ppbuffer);
    unsigned int i;

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
                    if(!pdbDeviceMenu) {
                        *options = (*options)^DBR_ENUM_STRS;/*Turn off option*/
                        break;
                    }
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

static void get_graphics(DBADDR *paddr, char **ppbuffer,
    rset *prset,long	*options)
{
	struct			dbr_grDouble grd;
	int			got_data=FALSE;

        grd.upper_disp_limit = grd.lower_disp_limit = 0.0;
	if( prset && prset->get_graphic_double ) {
		(*prset->get_graphic_double)(paddr,&grd);
		got_data=TRUE;
	}
	if( (*options) & (DBR_GR_LONG) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_grLong *pgr=(struct dbr_grLong*)pbuffer;
		    pgr->upper_disp_limit = (epicsInt32)grd.upper_disp_limit;
		    pgr->lower_disp_limit = (epicsInt32)grd.lower_disp_limit;
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

static void get_control(DBADDR *paddr, char **ppbuffer,
    rset *prset,long	*options)
{
	struct dbr_ctrlDouble	ctrld;
	int			got_data=FALSE;

        ctrld.upper_ctrl_limit = ctrld.lower_ctrl_limit = 0.0;
	if( prset && prset->get_control_double ) {
		(*prset->get_control_double)(paddr,&ctrld);
		got_data=TRUE;
	}
	if( (*options) & (DBR_CTRL_LONG) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_ctrlLong *pctrl=(struct dbr_ctrlLong*)pbuffer;
		    pctrl->upper_ctrl_limit = (epicsInt32)ctrld.upper_ctrl_limit;
		    pctrl->lower_ctrl_limit = (epicsInt32)ctrld.lower_ctrl_limit;
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

static void get_alarm(DBADDR *paddr, char **ppbuffer,
    rset *prset, long *options)
{
    char *pbuffer = *ppbuffer;
    struct dbr_alDouble ald = {epicsNAN, epicsNAN, epicsNAN, epicsNAN};
    long no_data = TRUE;

    if (prset && prset->get_alarm_double)
        no_data = prset->get_alarm_double(paddr, &ald);

    if (*options & DBR_AL_LONG) {
        struct dbr_alLong *pal = (struct dbr_alLong*) pbuffer;

        pal->upper_alarm_limit   = (epicsInt32) ald.upper_alarm_limit;
        pal->upper_warning_limit = (epicsInt32) ald.upper_warning_limit;
        pal->lower_warning_limit = (epicsInt32) ald.lower_warning_limit;
        pal->lower_alarm_limit   = (epicsInt32) ald.lower_alarm_limit;

        if (no_data)
            *options ^= DBR_AL_LONG; /*Turn off option*/

        *ppbuffer += dbr_alLong_size;
    }
    if (*options & DBR_AL_DOUBLE) {
        struct dbr_alDouble *pal = (struct dbr_alDouble*) pbuffer;

        pal->upper_alarm_limit   = ald.upper_alarm_limit;
        pal->upper_warning_limit = ald.upper_warning_limit;
        pal->lower_warning_limit = ald.lower_warning_limit;
        pal->lower_alarm_limit   = ald.lower_alarm_limit;

        if (no_data)
            *options ^= DBR_AL_DOUBLE; /*Turn off option*/

        *ppbuffer += dbr_alDouble_size;
    }
}

/*
 * This code relies on *poriginal being aligned and all increments done by the
 * blocks only changing the buffer pointer in a way that does not break alignment.
 */
static void getOptions(DBADDR *paddr, char **poriginal, long *options,
        void *pflin)
{
	db_field_log	*pfl= (db_field_log *)pflin;
    rset	*prset;
        short		field_type;
	dbCommon	*pcommon;
	char		*pbuffer = *poriginal;

        if (!pfl || pfl->type == dbfl_type_rec)
            field_type = paddr->field_type;
        else
            field_type = pfl->field_type;
	prset=dbGetRset(paddr);
	/* Process options */
	pcommon = paddr->precord;
	if( (*options) & DBR_STATUS ) {
	    unsigned short *pushort = (unsigned short *)pbuffer;

            if (!pfl || pfl->type == dbfl_type_rec) {
                *pushort++ = pcommon->stat;
                *pushort++ = pcommon->sevr;
            } else {
                *pushort++ = pfl->stat;
                *pushort++ = pfl->sevr;
            }
	    *pushort++ = pcommon->acks;
	    *pushort++ = pcommon->ackt;
	    pbuffer = (char *)pushort;
	}
	if( (*options) & DBR_UNITS ) {
	    memset(pbuffer,'\0',dbr_units_size);
	    if( prset && prset->get_units ){
		(*prset->get_units)(paddr, pbuffer);
		pbuffer[DB_UNITS_SIZE-1] = '\0';
	    } else {
		*options ^= DBR_UNITS; /*Turn off DBR_UNITS*/
	    }
	    pbuffer += dbr_units_size;
	}
	if( (*options) & DBR_PRECISION ) {
	    memset(pbuffer, '\0', dbr_precision_size);
	    if((field_type==DBF_FLOAT || field_type==DBF_DOUBLE)
	    &&  prset && prset->get_precision ){
                (*prset->get_precision)(paddr,(long *)pbuffer);
	    } else {
		*options ^= DBR_PRECISION; /*Turn off DBR_PRECISION*/
	    }
	    pbuffer += dbr_precision_size;
	}
	if( (*options) & DBR_TIME ) {
	    epicsUInt32 *ptime = (epicsUInt32 *)pbuffer;

            if (!pfl || pfl->type == dbfl_type_rec) {
                *ptime++ = pcommon->time.secPastEpoch;
                *ptime++ = pcommon->time.nsec;
            } else {
                *ptime++ = pfl->time.secPastEpoch;
                *ptime++ = pfl->time.nsec;
	    }
	    pbuffer = (char *)ptime;
	}
	if( (*options) & DBR_ENUM_STRS )
	    get_enum_strs(paddr, &pbuffer, prset, options);
	if( (*options) & (DBR_GR_LONG|DBR_GR_DOUBLE ))
	    get_graphics(paddr, &pbuffer, prset, options);
	if((*options) & (DBR_CTRL_LONG | DBR_CTRL_DOUBLE ))
	    get_control(paddr, &pbuffer, prset, options);
	if((*options) & (DBR_AL_LONG | DBR_AL_DOUBLE ))
	    get_alarm(paddr, &pbuffer, prset, options);
	*poriginal = pbuffer;
}

rset * dbGetRset(const struct dbAddr *paddr)
{
	struct dbFldDes *pfldDes = paddr->pfldDes;

	if(!pfldDes) return(0);
	return(pfldDes->pdbRecordType->prset);
}

long dbPutAttribute(
    const char *recordTypename, const char *name, const char *value)
{
    DBENTRY dbEntry;
    DBENTRY *pdbEntry = &dbEntry;
    long status = 0;

    if (!pdbbase)
        return S_db_notFound;
    if (!name) {
        status = S_db_badField;
        goto done;
    }
    if (!value)
        value = "";
    dbInitEntry(pdbbase, pdbEntry);
    status = dbFindRecordType(pdbEntry, recordTypename);
    if (!status)
        status = dbPutRecordAttribute(pdbEntry, name, value);
    dbFinishEntry(pdbEntry);
done:
    if (status)
        errMessage(status, "dbPutAttribute failure");
    return status;
}

int dbIsValueField(const struct dbFldDes *pdbFldDes)
{
    if (pdbFldDes->pdbRecordType->indvalFlddes == pdbFldDes->indRecordType)
        return TRUE;
    else
        return FALSE;
}

int dbGetFieldIndex(const struct dbAddr *paddr)
{
    return paddr->pfldDes->indRecordType;
}

/*
 *  Process a record if its scan field is passive.
 *     Will notify if processing is complete by callback.
 *       (only if you are interested in completion)
 */
long dbScanPassive(dbCommon *pfrom, dbCommon *pto)
{
    /* if not passive just return success */
    if (pto->scan != 0)
        return 0;

    if (pfrom && pfrom->ppn)
        dbNotifyAdd(pfrom,pto);
    return dbProcess(pto);
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
    rset *prset = precord->rset;
    dbRecordType *pdbRecordType = precord->rdes;
    unsigned char tpro = precord->tpro;
    char context[40] = "";
    long status = 0;
    int *ptrace;
    int	set_trace = FALSE;
    dbFldDes *pdbFldDes;
    int callNotifyCompletion = FALSE;

    ptrace = dbLockSetAddrTrace(precord);
    /*
     *  Note that it is likely that if any changes are made
     *   to dbProcess() corresponding changes will have to
     *   be made in the breakpoint handler.
     */

    /* see if there are any stopped records or breakpoints */
    if (lset_stack_count != 0) {
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
        if (!*ptrace) {
            *ptrace = 1;
            set_trace = TRUE;
        }
    }

    if (*ptrace) {
        /* Identify this thread's client from server layer */
        if (dbServerClient(context, sizeof(context))) {
            /* No client, use thread name */
            strncpy(context, epicsThreadGetNameSelf(), sizeof(context));
            context[sizeof(context) - 1] = 0;
        }
    }

    /* If already active dont process */
    if (precord->pact) {
        unsigned short monitor_mask;

        if (*ptrace)
            printf("%s: Active %s\n", context, precord->name);

        /* raise scan alarm after MAX_LOCK times */
        if ((precord->stat == SCAN_ALARM) ||
            (precord->lcnt++ < MAX_LOCK) ||
            (precord->sevr >= INVALID_ALARM)) goto all_done;

        recGblSetSevr(precord, SCAN_ALARM, INVALID_ALARM);
        monitor_mask = recGblResetAlarms(precord);
        monitor_mask |= DBE_VALUE|DBE_LOG;
        pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->indvalFlddes];
        db_post_events(precord,
                (void *)(((char *)precord) + pdbFldDes->offset),
                monitor_mask);
        goto all_done;
    }
    else
        precord->lcnt = 0;

    /*
     *  Check the record disable link.  A record will not be
     *    processed if the value retrieved through this link
     *    is equal to constant set in the record's disv field.
     */
    status = dbGetLink(&precord->sdis, DBR_SHORT, &precord->disa, 0, 0);

    /* if disabled check disable alarm severity and return success */
    if (precord->disa == precord->disv) {
        if (*ptrace)
            printf("%s: Disabled %s\n", context, precord->name);

        /*take care of caching and notifyCompletion*/
        precord->rpro = FALSE;
        precord->putf = FALSE;
        callNotifyCompletion = TRUE;

        /* raise disable alarm */
        if (precord->stat == DISABLE_ALARM)
            goto all_done;

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
    /* FIXME: put this in iocInit() !!! */
    if (!prset || !prset->process) {
        callNotifyCompletion = TRUE;
        precord->pact = 1;/*set pact so error is issued only once*/
        recGblRecordError(S_db_noRSET, (void *)precord, "dbProcess");
        status = S_db_noRSET;
        if (*ptrace)
            printf("%s: No RSET for %s\n", context, precord->name);
        goto all_done;
    }

    if (*ptrace)
        printf("%s: Process %s\n", context, precord->name);

    /* process record */
    status = prset->process(precord);

    /* Print record's fields if PRINT_MASK set in breakpoint field */
    if (lset_stack_count != 0) {
        dbPrint(precord);
    }

all_done:
    if (set_trace)
        *ptrace = 0;
    if (callNotifyCompletion && precord->ppn)
        dbNotifyCompletion(precord);

    return status;
}

/*
 *  Fill out a database structure (*paddr) for
 *    a record given by the name "pname."
 *
 *    Returns error codes from StaticLib module, not
 *        from dbAccess.
 */
long dbNameToAddr(const char *pname, DBADDR *paddr)
{
    DBENTRY dbEntry;
    dbFldDes *pflddes;
    long status = 0;
    short dbfType;

    if (!pname || !*pname || !pdbbase)
        return S_db_notFound;

    dbInitEntry(pdbbase, &dbEntry);
    status = dbFindRecordPart(&dbEntry, &pname);
    if (status) goto finish;

    if (*pname == '.') ++pname;
    status = dbFindFieldPart(&dbEntry, &pname);
    if (status == S_dbLib_fieldNotFound)
        status = dbGetAttributePart(&dbEntry, &pname);
    if (status) goto finish;

    pflddes = dbEntry.pflddes;
    dbfType = pflddes->field_type;

    paddr->precord = dbEntry.precnode->precord;
    paddr->pfield = dbEntry.pfield;
    paddr->pfldDes = pflddes;
    paddr->no_elements = 1;
    paddr->field_type  = dbfType;
    paddr->field_size  = pflddes->size;
    paddr->special     = pflddes->special;
    paddr->dbr_field_type = mapDBFToDBR[dbfType];

    if (paddr->special == SPC_DBADDR) {
        rset *prset = dbGetRset(paddr);

        /* Let record type modify paddr */
        if (prset && prset->cvt_dbaddr) {
            status = prset->cvt_dbaddr(paddr);
            if (status)
                goto finish;
            dbfType = paddr->field_type;
        }
    }

    /* Handle field modifiers */
    if (*pname++ == '$') {
        /* Some field types can be accessed as char arrays */
        if (dbfType == DBF_STRING) {
            paddr->no_elements = paddr->field_size;
            paddr->field_type = DBF_CHAR;
            paddr->field_size = 1;
            paddr->dbr_field_type = DBR_CHAR;
        } else if (dbfType >= DBF_INLINK && dbfType <= DBF_FWDLINK) {
            /* Clients see a char array, but keep original dbfType */
            paddr->no_elements = PVLINK_STRINGSZ;
            paddr->field_size = 1;
            paddr->dbr_field_type = DBR_CHAR;
        } else {
            status = S_dbLib_fieldNotFound;
            goto finish;
        }
    }

finish:
    dbFinishEntry(&dbEntry);
    return status;
}

void dbInitEntryFromAddr(struct dbAddr *paddr, DBENTRY *pdbentry)
{
    struct dbCommon *prec = paddr->precord;
    dbCommonPvt *ppvt = CONTAINER(prec, dbCommonPvt, common);

    memset((char *)pdbentry,'\0',sizeof(DBENTRY));

    pdbentry->pdbbase = pdbbase;
    pdbentry->precordType = prec->rdes;
    pdbentry->precnode = ppvt->recnode;
    pdbentry->pflddes = paddr->pfldDes;
    pdbentry->pfield = paddr->pfield;
    pdbentry->indfield = paddr->pfldDes->indRecordType;
}

void dbInitEntryFromRecord(struct dbCommon *prec, DBENTRY *pdbentry)
{
    dbCommonPvt *ppvt = CONTAINER(prec, dbCommonPvt, common);

    memset((char *)pdbentry,'\0',sizeof(DBENTRY));

    pdbentry->pdbbase = pdbbase;
    pdbentry->precordType = prec->rdes;
    pdbentry->precnode = ppvt->recnode;
}

long dbValueSize(short dbr_type)
{
    /* sizes for value associated with each DBR request type */
    static long size[] = {
        MAX_STRING_SIZE,             /* STRING       */
        sizeof(epicsInt8),           /* CHAR         */
        sizeof(epicsUInt8),          /* UCHAR        */
        sizeof(epicsInt16),          /* SHORT        */
        sizeof(epicsUInt16),         /* USHORT       */
        sizeof(epicsInt32),          /* LONG         */
        sizeof(epicsUInt32),         /* ULONG        */
        sizeof(epicsInt64),          /* INT64        */
        sizeof(epicsUInt64),         /* UINT64       */
        sizeof(epicsFloat32),        /* FLOAT        */
        sizeof(epicsFloat64),        /* DOUBLE       */
        sizeof(epicsEnum16)};        /* ENUM         */

    return(size[dbr_type]);
}


long dbBufferSize(short dbr_type, long options, long no_elements)
{
    long nbytes=0;

    nbytes += dbValueSize(dbr_type) * no_elements;
    if (options & DBR_STATUS)      nbytes += dbr_status_size;
    if (options & DBR_UNITS)       nbytes += dbr_units_size;
    if (options & DBR_PRECISION)   nbytes += dbr_precision_size;
    if (options & DBR_TIME)        nbytes += dbr_time_size;
    if (options & DBR_ENUM_STRS)   nbytes += dbr_enumStrs_size;
    if (options & DBR_GR_LONG)     nbytes += dbr_grLong_size;
    if (options & DBR_GR_DOUBLE)   nbytes += dbr_grDouble_size;
    if (options & DBR_CTRL_LONG)   nbytes += dbr_ctrlLong_size;
    if (options & DBR_CTRL_DOUBLE) nbytes += dbr_ctrlDouble_size;
    if (options & DBR_AL_LONG)     nbytes += dbr_alLong_size;
    if (options & DBR_AL_DOUBLE)   nbytes += dbr_alDouble_size;
    return(nbytes);
}
int dbLoadDatabase(const char *file, const char *path, const char *subs)
{
    return dbReadDatabase(&pdbbase, file, path, subs);
}

int dbLoadRecords(const char* file, const char* subs)
{
    int status = dbReadDatabase(&pdbbase, file, 0, subs);

    if (!status && dbLoadRecordsHook)
        dbLoadRecordsHook(file, subs);
    return status;
}


static long getLinkValue(DBADDR *paddr, short dbrType,
    char *pbuf, long *nRequest)
{
    dbCommon *precord = paddr->precord;
    dbFldDes *pfldDes = paddr->pfldDes;
    /* size of pbuf storage in bytes, including space for trailing nil */
    int maxlen;
    DBENTRY dbEntry;
    long status;
    long nReq = nRequest ? *nRequest : 1;

    /* dbFindRecord() below will always succeed as we have a
     * valid DBADDR, so no point to check again.
     * Request for zero elements always succeeds
     */
    if(!nReq)
        return 0;

    switch (dbrType) {
    case DBR_STRING:
        maxlen = MAX_STRING_SIZE;
        nReq = 1;
        break;

    case DBR_DOUBLE:    /* Needed for dbCa links */
        if (nRequest) *nRequest = 1;
        *(double *)pbuf = epicsNAN;
        return 0;

    case DBR_CHAR:
    case DBR_UCHAR:
        maxlen = nReq;
        break;
    default:
        return S_db_badDbrtype;
    }

    dbInitEntry(pdbbase, &dbEntry);
    status = dbFindRecord(&dbEntry, precord->name);
    if (!status) status = dbFindField(&dbEntry, pfldDes->name);
    if (!status) {
        const char *rtnString = dbGetString(&dbEntry);

        strncpy(pbuf, rtnString, maxlen-1);
        pbuf[maxlen-1] = 0;
        if(dbrType!=DBR_STRING)
            nReq = strlen(pbuf)+1;
        if(nRequest) *nRequest = nReq;
    }
    dbFinishEntry(&dbEntry);
    return status;
}

static long getAttrValue(DBADDR *paddr, short dbrType,
        char *pbuf, long *nRequest)
{
    int maxlen;
    long nReq = nRequest ? *nRequest : 1;

    if (!paddr->pfield) return S_db_badField;

    switch (dbrType) {
    case DBR_STRING:
        maxlen = MAX_STRING_SIZE;
        nReq = 1;
        break;

    case DBR_CHAR:
    case DBR_UCHAR:
        maxlen = nReq;
        break;

        /* else fall through ... */
    default:
        return S_db_badDbrtype;
    }

    strncpy(pbuf, paddr->pfield, maxlen-1);
    pbuf[maxlen-1] = 0;
    if(dbrType!=DBR_STRING)
        nReq = strlen(pbuf)+1;
    if(nRequest) *nRequest = nReq;
    return 0;
}

long dbGetField(DBADDR *paddr,short dbrType,
    void *pbuffer, long *options, long *nRequest, void *pflin)
{
    dbCommon *precord = paddr->precord;
    long status = 0;

    dbScanLock(precord);
    status = dbGet(paddr, dbrType, pbuffer, options, nRequest, pflin);
    dbScanUnlock(precord);
    return status;
}

long dbGet(DBADDR *paddr, short dbrType,
    void *pbuffer, long *options, long *nRequest, void *pflin)
{
    char *pbuf = pbuffer;
    void *pfieldsave = paddr->pfield;
    db_field_log *pfl = (db_field_log *)pflin;
    short field_type;
    long capacity, no_elements, offset;
    rset *prset;
    long status = 0;

    if (options && *options)
        getOptions(paddr, &pbuf, options, pflin);
    if (nRequest && *nRequest == 0)
        return 0;

    if (!pfl || pfl->type == dbfl_type_rec) {
        field_type = paddr->field_type;
        no_elements = capacity = paddr->no_elements;

        /* Update field info from record
         * may modify paddr->pfield
         */
        if (paddr->pfldDes->special == SPC_DBADDR &&
            (prset = dbGetRset(paddr)) &&
            prset->get_array_info) {
            status = prset->get_array_info(paddr, &no_elements, &offset);
        } else
            offset = 0;
    } else {
        field_type = pfl->field_type;
        no_elements = capacity = pfl->no_elements;
        offset = 0;
    }

    if (field_type >= DBF_INLINK && field_type <= DBF_FWDLINK) {
        status = getLinkValue(paddr, dbrType, pbuf, nRequest);
        goto done;
    }

    if (paddr->special == SPC_ATTRIBUTE) {
        status = getAttrValue(paddr, dbrType, pbuf, nRequest);
        goto done;
    }

    /* Check for valid request */
    if (INVALID_DB_REQ(dbrType) || field_type > DBF_DEVICE) {
        char message[80];

        sprintf(message, "dbGet: Request type is %d\n", dbrType);
        recGblDbaddrError(S_db_badDbrtype, paddr, message);
        status = S_db_badDbrtype;
        goto done;
    }

    if (offset == 0 && (!nRequest || no_elements == 1)) {
        if (nRequest)
            *nRequest = 1;
        if (!pfl || pfl->type == dbfl_type_rec) {
            status = dbFastGetConvertRoutine[field_type][dbrType]
                (paddr->pfield, pbuf, paddr);
        } else {
            DBADDR localAddr = *paddr; /* Structure copy */

            localAddr.field_type = pfl->field_type;
            localAddr.field_size = pfl->field_size;
            localAddr.no_elements = pfl->no_elements;
            if (pfl->type == dbfl_type_val)
                localAddr.pfield = (char *) &pfl->u.v.field;
            else
                localAddr.pfield = (char *)  pfl->u.r.field;
            status = dbFastGetConvertRoutine[field_type][dbrType]
                (localAddr.pfield, pbuf, &localAddr);
        }
    } else {
        long n;
        GETCONVERTFUNC convert;

        if (nRequest) {
            if (no_elements < *nRequest)
                *nRequest = no_elements;
            n = *nRequest;
        } else {
            n = 1;
        }
        convert = dbGetConvertRoutine[field_type][dbrType];
        if (!convert) {
            char message[80];

            sprintf(message, "dbGet: Missing conversion for [%d][%d]\n",
                    field_type, dbrType);
            recGblDbaddrError(S_db_badDbrtype, paddr, message);
            status = S_db_badDbrtype;
            goto done;
        }
        /* convert data into the caller's buffer */
        if (n <= 0) {
            ;/*do nothing*/
        } else if (!pfl || pfl->type == dbfl_type_rec) {
            status = convert(paddr, pbuf, n, capacity, offset);
        } else {
            DBADDR localAddr = *paddr; /* Structure copy */

            localAddr.field_type = pfl->field_type;
            localAddr.field_size = pfl->field_size;
            localAddr.no_elements = pfl->no_elements;
            if (pfl->type == dbfl_type_val)
                localAddr.pfield = (char *) &pfl->u.v.field;
            else
                localAddr.pfield = (char *)  pfl->u.r.field;
            status = convert(&localAddr, pbuf, n, capacity, offset);
        }

        if(!status && dbrType==DBF_CHAR && nRequest &&
                paddr->pfldDes && paddr->pfldDes->field_type==DBF_STRING)
        {
            /* long string ensure nil and truncate to actual length */
            long nReq = *nRequest;
            pbuf[nReq-1] = '\0';
            *nRequest = strlen(pbuf)+1;
        }
    }
done:
    paddr->pfield = pfieldsave;
    return status;
}

devSup* dbDTYPtoDevSup(dbRecordType *prdes, int dtyp) {
    return (devSup *)ellNth(&prdes->devList, dtyp+1);
}

devSup* dbDSETtoDevSup(dbRecordType *prdes, struct dset *pdset) {
    devSup *pdevSup = (devSup *)ellFirst(&prdes->devList);
    while (pdevSup) {
        if (pdset == pdevSup->pdset) return pdevSup;
        pdevSup = (devSup *)ellNext(&pdevSup->node);
    }
    return NULL;
}

static long dbPutFieldLink(DBADDR *paddr,
    short dbrType, const void *pbuffer, long nRequest)
{
    dbLinkInfo  link_info;
    DBADDR      *pdbaddr = NULL;
    dbCommon    *precord = paddr->precord;
    dbCommon    *lockrecs[2];
    dbLocker    locker;
    dbFldDes    *pfldDes = paddr->pfldDes;
    long        special = paddr->special;
    struct link *plink = (struct link *)paddr->pfield;
    const char  *pstring = (const char *)pbuffer;
    struct dsxt *old_dsxt = NULL;
    struct dset *new_dset = NULL;
    struct dsxt *new_dsxt = NULL;
    devSup      *new_devsup = NULL;
    long        status;
    int         isDevLink;
    short       scan;

    STATIC_ASSERT(DBLOCKER_NALLOC>=2);

    switch (dbrType) {
    case DBR_CHAR:
    case DBR_UCHAR:
        if (pstring[nRequest - 1] != '\0')
            return S_db_badDbrtype;
        break;

    case DBR_STRING:
        break;

    default:
        return S_db_badDbrtype;
    }

    status = dbParseLink(pstring, pfldDes->field_type, &link_info, 0);
    if (status)
        return status;

    if (link_info.ltype == PV_LINK &&
        (link_info.modifiers & (pvlOptCA | pvlOptCP | pvlOptCPP)) == 0) {
        DBADDR tempaddr;

        if (dbNameToAddr(link_info.target, &tempaddr)==0) {
            /* This will become a DB link. */
            pdbaddr = malloc(sizeof(*pdbaddr));
            if (!pdbaddr) {
                status = S_db_noMemory;
                goto cleanup;
            }
            *pdbaddr = tempaddr; /* struct copy */
        }
    }

    isDevLink = ellCount(&precord->rdes->devList) > 0 &&
                pfldDes->isDevLink;

    memset(&locker, 0, sizeof(locker));
    lockrecs[0] = precord;
    lockrecs[1] = pdbaddr ? pdbaddr->precord : NULL;
    dbLockerPrepare(&locker, lockrecs, 2);

    dbScanLockMany(&locker);

    scan = precord->scan;

    if (isDevLink) {
        new_devsup = dbDTYPtoDevSup(precord->rdes, precord->dtyp);
        if (new_devsup) {
            new_dset = new_devsup->pdset;
            new_dsxt = new_devsup->pdsxt;
        }
    }

    if (dbCanSetLink(plink, &link_info, new_devsup)) {
        /* link type mis-match prevents assignment */
        status = S_dbLib_badField;
        goto unlock;
    }

    if (isDevLink) {
        if (precord->dset) {
            devSup *old_devsup = dbDSETtoDevSup(precord->rdes, precord->dset);

            if (old_devsup)
                old_dsxt = old_devsup->pdsxt;
        }

        if (new_dsxt == NULL ||
            new_dsxt->add_record == NULL ||
            (precord->dset && old_dsxt == NULL) ||
            (old_dsxt && old_dsxt->del_record == NULL)) {
            status = S_db_noSupport;
            goto unlock;
        }

        if (scan == menuScanI_O_Intr) {
            scanDelete(precord);
            precord->scan = menuScanPassive;
        }

        if (old_dsxt) {
            status = old_dsxt->del_record(precord);
            if (status)
                goto restoreScan;
        }
    }

    if (dbLinkIsDefined(plink)) {
        dbRemoveLink(&locker, plink);   /* Clear out old link */
    }
    else if (!isDevLink) {
        status = S_db_badHWaddr;
        goto restoreScan;
    }

    if (special) status = dbPutSpecial(paddr, 0);

    if (!status) status = dbSetLink(plink, &link_info, new_devsup);

    if (!status && special) status = dbPutSpecial(paddr, 1);

    if (status) {
        if (isDevLink) {
            precord->dset = NULL;
            precord->pact = TRUE;
        }
        goto postScanEvent;
    }

    if (isDevLink) {
        precord->dpvt = NULL;
        precord->dset = new_dset;
        precord->pact = FALSE;

        status = new_dsxt->add_record(precord);
        if (status) {
            precord->dset = NULL;
            precord->pact = TRUE;
            goto postScanEvent;
        }
    }

    switch (plink->type) { /* New link type */
    case PV_LINK:
    case CONSTANT:
    case JSON_LINK:
        dbAddLink(&locker, plink, pfldDes->field_type, pdbaddr);
        break;

    case DB_LINK:
    case CA_LINK:
    case MACRO_LINK:
        break;  /* should never get here */

    default: /* Hardware address */
        if (!isDevLink) {
            status = S_db_badHWaddr;
            goto postScanEvent;
        }
        break;
    }
    db_post_events(precord, plink, DBE_VALUE | DBE_LOG);

restoreScan:
    if (isDevLink &&
        scan == menuScanI_O_Intr) { /* undo scanDelete() */
        precord->scan = scan;
        scanAdd(precord);
    }
postScanEvent:
    if (scan != precord->scan)
        db_post_events(precord, &precord->scan, DBE_VALUE | DBE_LOG);
unlock:
    dbScanUnlockMany(&locker);
    dbLockerFinalize(&locker);
cleanup:
    free(link_info.target);
    return status;
}

long dbPutField(DBADDR *paddr, short dbrType,
    const void *pbuffer, long nRequest)
{
    long	status = 0;
    long	special  = paddr->special;
    dbFldDes	*pfldDes = paddr->pfldDes;
    dbCommon	*precord = paddr->precord;
    short	dbfType  = paddr->field_type;

    if (special == SPC_ATTRIBUTE)
        return S_db_noMod;

    /*check for putField disabled*/
    if (precord->disp && paddr->pfield != &precord->disp)
        return S_db_putDisabled;

    if (dbfType >= DBF_INLINK && dbfType <= DBF_FWDLINK)
        return dbPutFieldLink(paddr, dbrType, pbuffer, nRequest);

    dbScanLock(precord);
    status = dbPut(paddr, dbrType, pbuffer, nRequest);
    if (status == 0) {
        if (paddr->pfield == &precord->proc ||
            (pfldDes->process_passive &&
             precord->scan == 0 &&
             dbrType < DBR_PUT_ACKT)) {
            if (precord->pact) {
                if (precord->tpro)
                    printf("%s: Active %s\n",
                        epicsThreadGetNameSelf(), precord->name);
                precord->rpro = TRUE;
            } else {
                /* indicate that dbPutField called dbProcess */
                precord->putf = TRUE;
                status = dbProcess(precord);
            }
        }
    }
    dbScanUnlock(precord);
    return status;
}

static long putAckt(DBADDR *paddr, const void *pbuffer, long nRequest,
    long no_elements, long offset)
{
    dbCommon *precord = paddr->precord;
    const unsigned short *ptrans = pbuffer;

    if (*ptrans == precord->ackt) return 0;
    precord->ackt = *ptrans;
    db_post_events(precord, &precord->ackt, DBE_VALUE | DBE_ALARM);
    if (!precord->ackt &&
        precord->acks > precord->sevr) {
        precord->acks = precord->sevr;
        db_post_events(precord, &precord->acks, DBE_VALUE | DBE_ALARM);
    }
    db_post_events(precord, NULL, DBE_ALARM);
    return 0;
}

static long putAcks(DBADDR *paddr, const void *pbuffer, long nRequest,
    long no_elements, long offset)
{
    dbCommon *precord = paddr->precord;
    const unsigned short *psev = pbuffer;

    if (*psev >= precord->acks) {
        precord->acks = 0;
        db_post_events(precord, &precord->acks, DBE_VALUE | DBE_ALARM);
        db_post_events(precord, NULL, DBE_ALARM);
    }
    return 0;
}

long dbPut(DBADDR *paddr, short dbrType,
    const void *pbuffer, long nRequest)
{
    dbCommon *precord = paddr->precord;
    short field_type  = paddr->field_type;
    long no_elements  = paddr->no_elements;
    long special      = paddr->special;
    void *pfieldsave  = paddr->pfield;
    rset *prset = dbGetRset(paddr);
    long status = 0;
    long offset;
    dbFldDes *pfldDes;
    int isValueField;

    if (special == SPC_ATTRIBUTE)
        return S_db_noMod;

    if (dbrType == DBR_PUT_ACKT && field_type <= DBF_DEVICE) {
        return putAckt(paddr, pbuffer, 1, 1, 0);
    } else if (dbrType == DBR_PUT_ACKS && field_type <= DBF_DEVICE) {
        return putAcks(paddr, pbuffer, 1, 1, 0);
    } else if (INVALID_DB_REQ(dbrType) || field_type > DBF_DEVICE) {
        char message[80];

        sprintf(message, "dbPut: Request type is %d", dbrType);
        recGblDbaddrError(S_db_badDbrtype, paddr, message);
        return S_db_badDbrtype;
    }

    if (special) {
        status = dbPutSpecial(paddr, 0);
        if (status) return status;
    }

    if (paddr->pfldDes->special == SPC_DBADDR &&
        prset && prset->get_array_info) {
        long dummy;

        status = prset->get_array_info(paddr, &dummy, &offset);
        /* paddr->pfield may be modified */
        if (status) goto done;
    } else
        offset = 0;

    if (no_elements <= 1) {
        status = dbFastPutConvertRoutine[dbrType][field_type](pbuffer,
            paddr->pfield, paddr);
        nRequest = 1;
    } else {
        if (no_elements < nRequest)
            nRequest = no_elements;
        status = dbPutConvertRoutine[dbrType][field_type](paddr, pbuffer,
            nRequest, no_elements, offset);
    }

    /* update array info */
    if (!status &&
        paddr->pfldDes->special == SPC_DBADDR &&
        prset && prset->put_array_info) {
        status = prset->put_array_info(paddr, nRequest);
    }

    /* Always do special processing if needed */
    if (special) {
        long status2 = dbPutSpecial(paddr, 1);
        if (status2) goto done;
    }
    if (status) goto done;

    /* Propagate monitor events for this field, */
    /* unless the field is VAL and PP is true. */
    pfldDes = paddr->pfldDes;
    isValueField = dbIsValueField(pfldDes);
    if (isValueField) precord->udf = FALSE;
    if (precord->mlis.count &&
        !(isValueField && pfldDes->process_passive))
        db_post_events(precord, pfieldsave, DBE_VALUE | DBE_LOG);
    /* If this field is a property (metadata) field,
     * then post a property change event (even if the field
     * didn't change).
     */
    if (precord->mlis.count && pfldDes->prop)
        db_post_events(precord, NULL, DBE_PROPERTY);
done:
    paddr->pfield = pfieldsave;
    return status;
}

