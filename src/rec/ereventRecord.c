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
 *      Author:		John Winans
 *      Date:           8/27/93
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

#include "ereventDefs.h"
#define GEN_SIZE_OFFSET
#include "ereventRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

#define STATIC	static

STATIC void ErEventMonitor(struct ereventRecord *pRec);

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
STATIC long ErEventInitRec(struct ereventRecord *, int);
STATIC long ErEventProc(struct ereventRecord *);
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

rset ereventRSET={
	RSETNUMBER,
	report,
	initialize,
	ErEventInitRec,
	ErEventProc,
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
epicsExportAddress(rset,ereventRSET);



STATIC long ErEventInitRec(struct ereventRecord *pRec, int pass)
{
  EreventDsetStruct	*pDset = (EreventDsetStruct *) pRec->dset;

  if (pass == 1)
  {
    /* Init the card via driver calls */
    /* Make sure we have a usable device support module */
    if (pDset == NULL)
    {
      recGblRecordError(S_dev_noDSET,(void *)pRec, "eg: ErEventInitRec");
      return(S_dev_noDSET);
    }
    if (pDset->number < 5)
    {
      recGblRecordError(S_dev_missingSup,(void *)pRec, "eg: ErEventInitRec");
      return(S_dev_missingSup);
    }
    if( pDset->initRec != NULL)
      return(*pDset->initRec)(pRec);
  }
  return(0);
}
/******************************************************************************
 ******************************************************************************/
STATIC long ErEventProc(struct ereventRecord *pRec)
{
  EreventDsetStruct  *pDset = (EreventDsetStruct *) pRec->dset;

  pRec->pact=TRUE;

  if (pRec->tpro > 10)
    printf("recErEvent::ErEventProc(%s) entered\n",  pRec->name);

  if (pDset->proc)
    (*pDset->proc)(pRec);

  /* Take care of time stamps and such */
  pRec->udf=FALSE;
  /* tsLocalTime(&pRec->time);*/
  recGblGetTimeStamp(pRec);

  /* Deal with monitor stuff */
  ErEventMonitor(pRec);
  /* process the forward scan link record */
  recGblFwdLink(pRec);

  pRec->pact=FALSE;
  return(0);
}

/******************************************************************************
 *
 ******************************************************************************/
STATIC void ErEventMonitor(struct ereventRecord *pRec)
{
  unsigned short  monitor_mask;

  monitor_mask = recGblResetAlarms(pRec);
  monitor_mask |= (DBE_VALUE | DBE_LOG);
  db_post_events(pRec, &pRec->val, monitor_mask);
  return;
}
