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
 */

/* This is a major revision of the original implementation of database access.*/

/* Global Database Access Routines
 *
 * dbCommonInit			Initialize dbCommon
 *
 * dbScanLock			Lock for scanning records
 * dbScanUnlock			Unlock for scanning records
 * dbScanLockInit		Initialize scan lock
 *
 * dbScanPassive		process if record is passively scanned
 * dbProcess			process a database record
 * dbNameToAddr			Given "pv<.field>" compute dbAddr
 */

#include	<vxWorks.h>
#include	<lstLib.h>
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
#include	<choice.h>
#include	<dbDefs.h>
#include	<dbBase.h>
#include	<dbAccess.h>
#include	<dbBkpt.h>
#include	<dbStaticLib.h>
#include	<dbScan.h>
#include	<dbCommon.h>
#include	<dbFldTypes.h>
#include 	<dbRecDes.h>
#include 	<dbRecType.h>
#include	<db_field_log.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>

extern struct dbBase *pdbBase;
extern long lset_stack_not_empty;

static short mapDBFToDBR[DBF_NTYPES] = { DBF_STRING, DBF_CHAR, DBF_UCHAR, DBF_SHORT, DBF_USHORT, 
     DBF_LONG, DBF_ULONG, DBF_FLOAT, DBF_DOUBLE, DBF_ENUM, DBF_ENUM, DBF_ENUM, DBF_ENUM, DBF_ENUM, 
     DBF_NOACCESS, DBF_NOACCESS, DBF_NOACCESS, DBF_NOACCESS};

/* Added for Channel Access Links */
long dbCaAddInlink();
long dbCaGetLink();
long dbCommonInit();

/*
 *  The number of consecutive attempts that can be made to process an
 *    active record before a SCAN_ALARM is raised.  Active records
 *    (records with the pact flag set) cannot be processed until
 *    that flag becomes unset.
 */
#define MAX_LOCK 10

/*
 *  The lock structure for each database lock set.  A lockset is the
 *    connected graph of all adjacent nodes to a record (with one
 *    exception, single-valued NPP NMS gets), where adjacent is defined
 *    as a record connected by a get, put, or forward link to the current
 *    record.
 */
struct scanLock {
	FAST_LOCK	lock;
	void		*precord;
	ULONG		start_time;
	int		task_id;
};

/*
 *  Structure that contains private information about the record
 *    locking mechanism.  It stores the total number of locksets
 *    in the database (computed by iocInit).  It also contains
 *    a pointer to the allocated array of all the scanLock
 *    structures.
 */
static struct {
	int		nset;		/* Number of sets */
	struct scanLock *pscanLock;	/*addr of array of struct scanLock */
} dbScanPvt;


/*
 *   Initialize the record's dbCommon structure.
 *      dbCommon is the set of all fields that are
 *      common to all record types.  These fields serve
 *      as a header to each record's structure.
 */
long dbCommonInit(struct dbCommon *precord, int pass)
{
    long status;

    if (pass == 0)
        return(0);

   /*
    *  Init the record disable link.  A record will not be
    *    processed if the value retrieved through this link
    *    is equal to constant set in the record's disv field.
    */
    status = recGblInitFastInLink(&(precord->sdis), (void *) precord, DBF_SHORT, "DISA");

    if (status)
       recGblRecordError(status, (void *) precord, "dbCommonInit: SDIS");

   /*
    *  Initialize the time stamp link.
    */
    status = recGblInitFastInLink(&(precord->tsel), (void *) precord, DBF_SHORT, "TSE");

    if (status)
       recGblRecordError(status, (void *)precord, "dbCommonInit: TSEL");

    return(0);
} /* end dbCommonInit() */

/*
 *  Wait on a semaphore for access to the record's lockset.
 *    A lockset should be locked for actions including but
 *    not limited to database processing.
 */
void dbScanLock(struct dbCommon *precord)
{
	struct scanLock *pscanLock;
	short		lset=((struct dbCommon *)precord)->lset - 1;

       /* Move range check to iocInit */
	if(lset < 0 || lset >= dbScanPvt.nset) {
		errMessage(S_db_badLset, "Lock Set out of range:dbScanLock");
		taskSuspend(taskIdSelf());
	}
	pscanLock = dbScanPvt.pscanLock + lset;
	FASTLOCK(&pscanLock->lock);
	pscanLock->start_time = tickGet();
	pscanLock->task_id = taskIdSelf();
	pscanLock->precord = (void *)precord;
	return;
}

/*
 *  Unlock the semaphore.  Allow other processes to perform
 *    operations on the records within the lockset.
 */
void dbScanUnlock(struct dbCommon *precord)
{
	struct scanLock *pscanLock;
	short		lset=((struct dbCommon *)precord)->lset - 1;

       /* Put check in iocInit() */
	if(lset<0 || lset>=dbScanPvt.nset) {
		errMessage(S_db_badLset,"Lock Set out of range:dbScanUnlock");
		taskSuspend(taskIdSelf());
	}
	pscanLock = dbScanPvt.pscanLock + lset;
	pscanLock->precord = NULL;
	FASTUNLOCK(&pscanLock->lock);
	return;
}

/*
 *  Initialize the locking mechanism for all locksets.
 */
void dbScanLockInit(int nset)
{
	struct scanLock	*pscanLock;
	int i;

	dbScanPvt.nset = nset;

       /*
        *  Allocate one scan lock structure for each
        *     lockset in the database.
        */
	pscanLock = calloc((size_t)nset, (size_t)sizeof(struct scanLock));

	dbScanPvt.pscanLock = pscanLock;

       /*
        *  Initialize scan lock structure for
        *    all locksets.  This includes
        *    initializing the FASTLOCK semaphore.
        */
	for (i=0; i<nset; i++, pscanLock++) {
	    FASTLOCKINIT(&pscanLock->lock);
	    pscanLock->precord=NULL;
	    pscanLock->start_time=0;
	    pscanLock->task_id=0;
	}
	return;
}

/*
 *  Process a record if its scan field is passive.
 *     Will notify if processing is complete by callback.
 *       (only if you are interested in completion)
 */
long dbScanPassive(struct dbCommon *pfrom, struct dbCommon *pto)
{
    long status;
	
    /* if not passive just return success */
    if(pto->scan != 0) return(0);

    if(pfrom && pfrom->ppn) dbNotifyAdd(pfrom,pto);
    status = dbProcess(pto);
    return(status);
}

/*KLUDGE: Following needed so that dbPutLink to PROC field works correctly*/
long dbScanLink(struct dbCommon *pfrom, struct dbCommon *pto)
{
    long status;

    if(pfrom && pfrom->ppn) dbNotifyAdd(pfrom,pto);
    status = dbProcess(pto);
    return(status);
}

/*
 *   Process the record.
 *     1.  Check for breakpoints.
 *     2.  Check the process active flag (PACT).
 *     3.  Check the disable link.
 *     4.  Check the RSET (record support entry table) exists.(PUT THIS IN IOCIN
IT)
 *     5.  Run the process routine specific to the record type.
 *     6.  Check to see if record contents should be automatically printed.
 */
long dbProcess(struct dbCommon *precord)
{
	struct rset	*prset;
	unsigned char	tpro=precord->tpro;
	short		lset = precord->lset;
	long		status = 0;
	static char 	trace=0;
	static int	trace_lset=0;
	int		set_trace=FALSE;

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
	        struct rset     *prset;
		struct valueDes valueDes;
		unsigned short	monitor_mask;

		if (trace && trace_lset==lset)
			printf("active:    %s\n",precord->name);
		/* raise scan alarm after MAX_LOCK times */
		if (precord->stat==SCAN_ALARM)
                    goto all_done;

		if (precord->lcnt++ !=MAX_LOCK)
                    goto all_done;

		if (precord->sevr>=INVALID_ALARM)
                    goto all_done;

		recGblSetSevr(precord, SCAN_ALARM, INVALID_ALARM);
		monitor_mask = recGblResetAlarms(precord);
		monitor_mask |= DBE_VALUE;
	        prset=(struct rset *)precord->rset;

		if (prset && prset->get_value) {
			(*prset->get_value)(precord, &valueDes);
			db_post_events(precord, valueDes.pvalue, monitor_mask);
		}
		goto all_done;
	}
        else precord->lcnt = 0;

       /*
        *  Check the record disable link.  A record will not be
        *    processed if the value retrieved through this link
        *    is equal to constant set in the record's disv field.
        */
        status = recGblGetFastLink(&(precord->sdis), (void *)precord, &(precord->disa));

	/* if disabled check disable alarm severity and return success */
	if (precord->disa == precord->disv) {
		struct valueDes valueDes;

		if (trace && trace_lset==lset)
			printf("disabled:  %s\n",precord->name);

		/*take care of caching and notifyCompletion*/
		precord->rpro = FALSE;
		if (precord->ppn)
                    dbNotifyCompletion(precord);

		/* raise disable alarm */
		if (precord->stat==DISABLE_ALARM)
                    goto all_done;

		precord->sevr = precord->diss;
		precord->stat = DISABLE_ALARM;
		precord->nsev = 0;
		precord->nsta = 0;

		/* anyone waiting for an event on this record?*/
		if (precord->mlis.count == 0)
                    goto all_done;

		db_post_events(precord, &precord->stat, DBE_VALUE);
		db_post_events(precord, &precord->sevr, DBE_VALUE);
	        prset=(struct rset *)precord->rset;

		if (prset && prset->get_value) {
		    (*prset->get_value)(precord, &valueDes);
		    db_post_events(precord, valueDes.pvalue, DBE_VALUE|DBE_ALARM);
		}
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

	if (trace && trace_lset==lset)
		printf("process:   %s\n", precord->name);

	/* process record */
	status = (*prset->process)(precord);

        /*
         *  Print record's fields if PRINT_MASK set in breakpoint field
         */
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
long dbNameToAddr(char *pname,struct dbAddr *paddr)
{
	DBENTRY		dbEntry;
	long		status=0;
	struct rset	*prset;
	struct fldDes	*pflddes;

	dbInitEntry(pdbBase,&dbEntry);
	if(status = dbFindRecord(&dbEntry,pname)) return(status);
	paddr->precord = dbEntry.precnode->precord;
	paddr->record_type = dbEntry.record_type;
	if(!dbEntry.pfield) {
		if(status=dbFindField(&dbEntry,"VAL"))return(status);
	}
	paddr->pfield = dbEntry.pfield;
	pflddes = dbEntry.pflddes;
	paddr->pfldDes = (void *)pflddes;
	paddr->field_type = pflddes->field_type;
	paddr->dbr_field_type = mapDBFToDBR[pflddes->field_type];
	paddr->field_size = pflddes->size;
	paddr->choice_set = pflddes->choice_set;
	paddr->special = pflddes->special;

	/*if special is SPC_DBADDR then call cvt_dbaddr		*/
	/*it may change pfield,no_elements,field_type,dbr_field_type,*/
	/*field_size,and special*/
	paddr->no_elements=1;
	if( ((paddr->special)==SPC_DBADDR)
	  && (prset=GET_PRSET(pdbBase->precSup,paddr->record_type))
	  && (prset->cvt_dbaddr) ) 
		status = (*prset->cvt_dbaddr)(paddr);

	return(status);
}

/*
 *  Fast Database Links
 *
 *  Currently, single-valued GET links have improved by a factor of 4,
 *     and PUT links by a factor of 2.
 *
 *  The check for CONSTANT links is done in recSup.h with macros.  This
 *     removes an expensive subroutine call when it is not necessary to
 *     call one.  This is a performance improvement over all of record
 *     support.
 *
 *  Wish List --
 *
 *  1.  CA optimizations ?
 *  2.  Array Support.
 *  3.  Support something with the same callable interface as dbGetLink()
 *        and dbPutLink() that would interact with channel access (and
 *        support OPTIONS for instance ...).
 *
 *   CA/Arrays - can also be set as different "flavor" functions
 *     by adding another function to the link structure, then
 *     recGblInitFastInLink() or recGblInitFastOutLink() can figure
 *     out which "flavor" of the put or get to use.
 *
 *   Arrays can be efficiently supported by providing conversion
 *     routines suited for them.
 *
 *   Flavor functions would also be able to improve the performance of
 *     both GET and PUT db_links more, since checking for PP/MS etc.
 *     will no longer be necessary.
 *
 *   If 2,3 are granted, it is almost possible to replace all the
 *     functionality of previous link routines in dbLink.c with
 *     dbFastLink.c and dbFastLinkConv.c.  And the code will be
 *     smaller, easier to understand, more maintainable, and more
 *     efficient.
 */


/*
 *  Get value through fast input link.
 *
 *  1.  Check for and call channel access routine instead if appropriate
 *  2.  Set PACT flag
 *  3.  Process record if process passive field set
 *  4.  Maximize severity through the link if flag set
 *  5.  Reset PACT flag
 */
long dbFastLinkGet(
     struct link *plink,
     struct dbCommon *precord,
     void *pdest)
{
  long status = 0;
  char pact = precord->pact;

  precord->pact = TRUE;

 /*
  *  Test for CA_LINK
  */
  if (plink->type == CA_LINK) {
       status = dbCaGetLink(plink);
  }
  else {
       struct db_link *pdb_link = &(plink->value.db_link);
       struct dbAddr *pdb_addr = (struct dbAddr *) (pdb_link->pdbAddr);

       if (pdb_link->process_passive) {
          status = dbScanPassive(precord, pdb_addr->precord);

          if (status) {
             precord->pact = pact;
             return(status);
          }
       }

       if (pdb_link->maximize_sevr) {
           recGblSetSevr(precord, LINK_ALARM, pdb_addr->precord->sevr);
       }

      /*
       *  Call conversion routine
       */
       status = (*(pdb_link->conversion))(pdb_addr->pfield, pdest, pdb_addr);
  }

  precord->pact = pact;
  return(status);
}

/*
 *  Put value through fast link
 *
 *  1. Check for and call channel access routine instead if appropriate
 *  2. Set PACT flag
 *  3. Propagate Maximize severity
 *  4. Check if put was made to force processing field
 *  5. Process record if process passive field set
 *  6. Check field for special processing
 *
 *  pl_record stands for pointer to local record.
 *  pr_record stands for pointer to remote record.
 */
long dbFastLinkPut(
     struct link *plink,
     struct dbCommon *pl_record,
     void *psource)
{
  long status = 0;
  char pact = pl_record->pact;

  pl_record->pact = TRUE;

 /*
  *  Check for CA Link
  */
  if (plink->type == CA_LINK) {
       long options = 0;
       long nRequest = 1;

       dbCaPutLink(plink, &options, &nRequest);
  }
  else {
       struct db_link *pdb_link = &(plink->value.db_link);
       struct dbAddr *paddr = (struct dbAddr *) (pdb_link->pdbAddr);
       struct dbCommon *pdest = (struct dbCommon *) paddr->precord; 
       struct fldDes *p_fldDes = (struct fldDes *) (paddr->pfldDes);
       long special = paddr->special;
       unsigned short acks = pdest->acks;
       long *pfield_name;
       long int (*pspecial)() = NULL;
       struct rset *prset;
       static char val[4] = {'V','A','L',' '};
       long *pval = (long *) &val[0];

       prset = GET_PRSET(pdbBase->precSup, paddr->record_type);

       if (special) {
           if (special < 100) {
               if (special == SPC_NOMOD) {
                    pl_record->pact = pact;
                    return(S_db_noMod);
               }
               else { 
                  if (special == SPC_SCAN) {
                    scanDelete(pdest);
                  }
               }
           }
           else {
                  if (prset && (pspecial = (prset->special))) {
                     status = (*pspecial)(paddr, 0);
                     if (status) {
                         pl_record->pact = pact;
                         return(status);
                     }
                  }
                  else {
                     recGblRecSupError(S_db_noSupport, paddr, "dbPut", "special");
                     pl_record->pact = pact;
                     return(S_db_noSupport);
                  }
           }
       }

      /*
       *  Call conversion routine
       */
       status = (*(pdb_link->conversion))(psource, paddr->pfield, paddr);

       if (status) {
            pl_record->pact = pact;
            return(status);
       }

      /*
       *  Check for special processing
       */
       if (special) {
          if (special < 100) { /*global processing*/
               if (special == SPC_SCAN) {
                  scanAdd(pdest);
               }
               else if (special == SPC_ALARMACK) {
                  if (paddr->pfield == (void *) &pdest->acks) {
                     if (acks > 0 && acks <= pdest->acks) {
                         pdest->acks = 0;
                         db_post_events(pdest, &pdest->acks, DBE_VALUE);
                     }
                     else { /* Undo change */
                         pdest->acks = acks;
                     }
                  }
                  else if (paddr->pfield == (void *) &pdest->ackt) {
                         if (!pdest->ackt && pdest->acks > pdest->sevr) {
                            pdest->acks = pdest->sevr;
                            db_post_events(pdest, &pdest->acks, DBE_VALUE);
                         }
                  }
               }
               else if (special == SPC_AS) {
                   asChangeGroup(&pdest->asp, pdest->asg);
               }
          }
          else {
               status = (*pspecial)(paddr, 1);
               if (status) {
                  pl_record->pact = pact;
                  return(status);
               }
          }
       }

      /*
       *  Propagate events for this field
       *    if the field is VAL and process_passive is true
       *    don't propagate.
       */

       p_fldDes = (struct fldDes *) (paddr->pfldDes);
       pfield_name = (long *) &(p_fldDes->fldname[0]);

      /* if field is val set udf FALSE */
       if (*pval == *pfield_name)
            pdest->udf = FALSE;

       if (pdest->mlis.count &&
                ((*pval != *pfield_name) || (!p_fldDes->process_passive))) {
            db_post_events(pdest, paddr->pfield, DBE_VALUE);
       }

       if (pdb_link->maximize_sevr) {
           recGblSetSevr(pdest, LINK_ALARM, pl_record->sevr);
       }

       if (status) {
           pl_record->pact = pact;
           return(status);
       }

       if ((paddr->pfield == (void *) &pdest->proc) ||
           (pdb_link->process_passive && pdest->scan == 0)) {

          /*
           *  Note: If ppn then dbNotifyCancel will handle reprocessing
           *    if dbPutField caused asyn record to process ask for reprocessing
           */
           if (!pl_record->ppn && pdest->putf)
              pdest->rpro = TRUE;

          /* otherwise ask for the record to be processed*/
           else status = dbScanLink(pl_record, pdest);
       }

       if (status)
          recGblRecordError(status, (void *) psource, "dbPutLink");
 }

 pl_record->pact = pact;
 return(status);
}


