/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *
 *      Author: John Winans
 *      Date:            8/27/93
 *
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
#define epicsExportSharedSymbols

#define GEN_SIZE_OFFSET
#include "egRecord.h"
#undef  GEN_SIZE_OFFSET
#include "egDefs.h"

#define STATIC	static

STATIC void EgMonitor(struct egRecord *pRec);

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
STATIC long EgInitRec(struct egRecord *, int);
STATIC long EgProc(struct egRecord *);
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

epicsShareDef struct rset egRSET={
	RSETNUMBER,
	report,
	initialize,
	EgInitRec,
	EgProc,
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



STATIC long EgInitRec(struct egRecord *pRec, int pass)
{
  EgDsetStruct	*pDset = (EgDsetStruct *) pRec->dset;

  if (pass == 1)
  {
    /* Init the card via driver calls */
    /* Make sure we have a usable device support module */
    if (pDset == NULL)
    {
      recGblRecordError(S_dev_noDSET,(void *)pRec, "eg: EgInitRec");
      return(S_dev_noDSET);
    }
    if (pDset->number < 5)
    {
      recGblRecordError(S_dev_missingSup,(void *)pRec, "eg: EgInitRec");
      return(S_dev_missingSup);
    }
    if( pDset->initRec != NULL) 
      return(*pDset->initRec)(pRec);
  }
  pRec->taxi = 0;

  return(0);
}
/******************************************************************************
 ******************************************************************************/
STATIC long EgProc(struct egRecord *pRec)
{
  EgDsetStruct  *pDset = (EgDsetStruct *) pRec->dset;

  pRec->pact=TRUE;

  pRec->ltax = pRec->taxi;	/* The only monitorable field we can change */

  if (pRec->tpro > 10)
    printf("recEg::EgProc(%s) entered\n",  pRec->name);

  if (pDset->proc != NULL)
    (*pDset->proc)(pRec);

  /* Take care of time stamps and such */
  pRec->udf=FALSE;
  /*tsLocalTime(&pRec->time);*/
  recGblGetTimeStamp(pRec);

  /* Deal with monitor stuff */
  EgMonitor(pRec);
  /* process the forward scan link record */
  recGblFwdLink(pRec);

  pRec->pact=FALSE;
  return(0);
}

/******************************************************************************
 *
 ******************************************************************************/
STATIC void EgMonitor(struct egRecord *pRec)
{
  unsigned short  monitor_mask;

  monitor_mask = recGblResetAlarms(pRec);
  monitor_mask |= (DBE_VALUE | DBE_LOG);
  db_post_events(pRec, &pRec->val, monitor_mask);

  if (pRec->taxi != pRec->ltax)
    db_post_events(pRec, &pRec->taxi, monitor_mask);

  return;
}
