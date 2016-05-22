/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *      Author:	John Winans
 *      Date:	09-21-92
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#include "callback.h"

#define GEN_SIZE_OFFSET
#include "seqRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

int	seqRecDebug = 0;

/* Create RSET - Record Support Entry Table*/
static long init_record(seqRecord *prec, int pass);
static long process(seqRecord *prec);
static int processNextLink(seqRecord *prec);
static long asyncFinish(seqRecord *prec);
static void processCallback(CALLBACK *arg);
static long get_precision(dbAddr *paddr, long *pprecision);

rset seqRSET={
	RSETNUMBER,
	NULL,			/* report */
	NULL,			/* initialize */
	init_record,		/* init_record */
	process,		/* process */
	NULL,			/* special */
	NULL,			/* get_value */
	NULL,			/* cvt_dbaddr */
	NULL,			/* get_array_info */
	NULL,			/* put_array_info */
	NULL,			/* get_units */
	get_precision,		/* get_precision */
	NULL,			/* get_enum_str */
	NULL,			/* get_enum_strs */
	NULL,			/* put_enum_str */
	NULL,			/* get_graphic_double */
	NULL,			/* get_control_double */
	NULL 			/* get_alarm_double */

};
epicsExportAddress(rset,seqRSET);

/* Total number of link-groups in a sequence record */
#define NUM_LINKS	10
#define SELN_BIT_MASK	~(0xffff << NUM_LINKS)

/* This is what a link-group looks like in a sequence record */
typedef struct	linkDesc {
  double          dly;	/* Delay value (in seconds) */
  struct link     dol;	/* Where to fetch the input value from */
  double          dov;	/* If dol is CONSTANT, this is the CONSTANT value */
  struct link     lnk;	/* Where to put the value from dol */
}linkDesc;

/* Callback structure used by the watchdog function to queue link processing */
typedef struct callbackSeq {
  CALLBACK	callback;	/* used for the callback task */
  seqRecord     *pseqRecord;
  linkDesc 	*plinks[NUM_LINKS+1]; /* Pointers to links to process */
  int		index;
}callbackSeq;


/*****************************************************************************
 *
 * Initialize a sequence record.
 *
 * Allocate the callback request structure (tacked on dpvt.)
 * Initialize watch-dog ID
 * Initialize SELN based on the link-type of SELL
 * If SELL is a CA_LINK, inform CA about it
 * For each constant input link, fill in the DOV field
 *
 ******************************************************************************/
static long init_record(seqRecord *prec, int pass)
{
    int		index;
    linkDesc      *plink;
    callbackSeq *pcallbackSeq;

    if (pass==0) return(0);

    if (seqRecDebug > 5)
      printf("init_record(%s) entered\n", prec->name);

    /* Allocate a callback structure for use in processing */
    pcallbackSeq  = (callbackSeq *)calloc(1,sizeof(callbackSeq));
    pcallbackSeq->pseqRecord = prec;
    callbackSetCallback(processCallback,&pcallbackSeq->callback);
    callbackSetUser(pcallbackSeq,&pcallbackSeq->callback);
    callbackSetPriority(prec->prio,&pcallbackSeq->callback);
    prec->dpvt = (void *)pcallbackSeq;

    /* Get link selection if sell is a constant and nonzero */
    if (prec->sell.type==CONSTANT)
    {
      if (seqRecDebug > 5)
        printf("init_record(%s) SELL is a constant\n", prec->name);
      recGblInitConstantLink(&prec->sell,DBF_USHORT,&prec->seln);
    }

  /* Copy over ALL the input link constants here */
  plink = (linkDesc *)(&(prec->dly1));

  index = 0;
  while (index < NUM_LINKS)
  {

    if (plink->dol.type == CONSTANT)
	recGblInitConstantLink(&plink->dol,DBF_DOUBLE,&plink->dov);

    index++;

    plink++;
  }

  return(0);
}

/*****************************************************************************
 *
 * Process a sequence record.
 *
 * If is async completion phase
 *   call asyncFinish() to finish things up
 * else
 *   figure out selection mechanism
 *   build the correct mask value using the mode and the selection value
 *   build a list of pointers to the selected link-group structures
 *   If there are no links to process
 *     call asyncFinish() to finish things up
 *   else
 *     call processNextLink() to schecule a delay for the first link-group
 *
 *
 * NOTE:
 *   dbScanLock is already held for prec before this function is called.
 *
 *   We do NOT call processNextLink() if there is nothing to do, this prevents
 *   us from calling dbProcess() recursively.
 *
 ******************************************************************************/
static long process(seqRecord *prec)
{
  callbackSeq	*pcb = (callbackSeq *) (prec->dpvt);
  linkDesc	*plink;
  unsigned short	lmask;
  int			tmp;

  if(seqRecDebug > 10)
    printf("seqRecord: process(%s) pact = %d\n", prec->name, prec->pact);

  if (prec->pact)
  { /* In async completion phase */
    asyncFinish(prec);
    return(0);
  }
  prec->pact = TRUE;

  /* Reset the PRIO in case it was changed */
  callbackSetPriority(prec->prio,&pcb->callback);

  /*
   * We should not bother supporting seqSELM_All or seqSELM_Specified
   * they can both be supported by simply providing seqSELM_Mask.
   * build the proper mask using these other silly modes if necessary.
   */

  if (prec->selm == seqSELM_All)
  {
    lmask = (unsigned short) SELN_BIT_MASK;
  }
  else
  { 
    /* Fill in the SELN field */
    if (prec->sell.type != CONSTANT)
    {
      dbGetLink(&(prec->sell), DBR_USHORT, &(prec->seln), 0,0);
    }
    if  (prec->selm == seqSELM_Specified)
    {
      if(prec->seln>10)
      { /* Invalid selection number */
        recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
        return(asyncFinish(prec));
      }
      if (prec->seln == 0)
        return(asyncFinish(prec));	/* Nothing selected */

      lmask = 1;
      lmask <<= prec->seln - 1;
    }
    else if (prec->selm == seqSELM_Mask)
    {
      lmask = (prec->seln) & SELN_BIT_MASK;
    }
    else
    { /* Invalid selection option */
      recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
      return(asyncFinish(prec));
    }
  }
  /* Figure out which links are going to be processed */
  pcb->index = 0;
  plink = (linkDesc *)(&(prec->dly1));
  tmp = 1;
  while (lmask)
  {
    if (seqRecDebug > 4)
      printf("seqRec-process Checking link %d - lnk %d dol %d\n", tmp,
      plink->lnk.type, plink->dol.type);

    if ((lmask & 1) && ((plink->lnk.type != CONSTANT)||(plink->dol.type != CONSTANT)))
    {
      if (seqRecDebug > 4)
	printf("  seqRec-process Adding link %d at index %d\n", tmp, pcb->index);

      pcb->plinks[pcb->index] = plink;
      pcb->index++;
    }
    lmask >>= 1;
    plink++;
    tmp++;
  }
  pcb->plinks[pcb->index] = NULL;	/* mark the bottom of the list */

  if (!pcb->index)
  { /* There was nothing to do, finish record processing here */
    return(asyncFinish(prec));
  }

  pcb->index = 0;
  /* Start doing the first forward link (We have at least one for sure) */
  processNextLink(prec);

  return(0);
}

/*****************************************************************************
 *
 * Find the next link-group that needs processing.
 *
 * If there are no link-groups left to process
 *   call bdProcess() to complete the async record processing.
 * else
 *   if the delay is > 0 seconds
 *     schedule the watch dog task to wake us up later
 *   else
 *     invoke the watch-dog wakeup routine now
 *
 *
 * NOTE:
 *   dbScanLock is already held for prec before this function is called.
 *
 ******************************************************************************/
static int processNextLink(seqRecord *prec)
{
  callbackSeq   *pcb = (callbackSeq *) (prec->dpvt);

  if (seqRecDebug > 5)
    printf("processNextLink(%s) looking for work to do, index = %d\n", prec->name, pcb->index);

  if (pcb->plinks[pcb->index] == NULL)
  {
    /* None left, finish up. */
    (*(struct rset *)(prec->rset)).process(prec);
  }
  else
  {
    if (pcb->plinks[pcb->index]->dly > 0.0)
    {
      callbackRequestDelayed( &pcb->callback,pcb->plinks[pcb->index]->dly);
    }
    else
    {
      /* No delay, do it now.  Avoid recursion by using the callback task */
      callbackRequest(&pcb->callback);
    }
  }
  return(0);
}

/*****************************************************************************
 *
 * Finish record processing by posting any events and processing forward links.
 *
 * NOTE:
 *   dbScanLock is already held for prec before this function is called.
 *
 ******************************************************************************/
static long asyncFinish(seqRecord *prec)
{
  unsigned short MonitorMask;

  if (seqRecDebug > 5)
    printf("asyncFinish(%s) completing processing\n", prec->name);
  prec->udf = FALSE;

  recGblGetTimeStamp(prec);

  MonitorMask = recGblResetAlarms(prec);

  if (MonitorMask)
    db_post_events(prec, &prec->val, MonitorMask);

  /* process the forward scan link record */
  recGblFwdLink(prec);

  prec->pact = FALSE;

  return(0);
}

/*****************************************************************************
 *
 * Link-group processing function.
 *
 * if the input link is not a constant
 *   call dbGetLink() to get the link value
 * else
 *   get the value from the DOV field
 * call dbPutLink() to forward the value to destination location
 * call processNextLink() to schedule the processing of the next link-group
 *
 * NOTE:
 *   dbScanLock is NOT held for prec when this function is called!!
 *
 ******************************************************************************/
static void processCallback(CALLBACK *arg)
{
  callbackSeq *pcb;
  seqRecord *prec;
  double	myDouble;

  callbackGetUser(pcb,arg);
  prec = pcb->pseqRecord;
  dbScanLock((struct dbCommon *)prec);

  if (seqRecDebug > 5)
    printf("processCallback(%s) processing field index %d\n", prec->name, pcb->index+1);

  /* Save the old value */
  myDouble = pcb->plinks[pcb->index]->dov;

  dbGetLink(&(pcb->plinks[pcb->index]->dol), DBR_DOUBLE,
	&(pcb->plinks[pcb->index]->dov),0,0);

  recGblGetTimeStamp(prec);

  /* Dump the value to the destination field */
  dbPutLink(&(pcb->plinks[pcb->index]->lnk), DBR_DOUBLE,
	&(pcb->plinks[pcb->index]->dov),1);

  if (myDouble != pcb->plinks[pcb->index]->dov)
  {
    if (seqRecDebug > 0)
      printf("link %d changed from %f to %f\n", pcb->index, myDouble, pcb->plinks[pcb->index]->dov);
    db_post_events(prec, &pcb->plinks[pcb->index]->dov, DBE_VALUE|DBE_LOG);
  }
  else
  {
    if (seqRecDebug > 0)
      printf("link %d not changed... %f\n", pcb->index, pcb->plinks[pcb->index]->dov);
  }

  /* Find the 'next' link-seq that is ready for processing. */
  pcb->index++;
  processNextLink(prec);

  dbScanUnlock((struct dbCommon *)prec);
  return;
}

/*****************************************************************************
 *
 * Return the precision value from PREC
 *
 *****************************************************************************/
static long get_precision(dbAddr *paddr, long *pprecision)
{
    seqRecord	*prec = (seqRecord *) paddr->precord;

    *pprecision = prec->prec;

    if(paddr->pfield < (void *)&prec->val)
	return 0;			/* Field is NOT in dbCommon */

    recGblGetPrec(paddr, pprecision);	/* Field is in dbCommon */
    return 0;
}
