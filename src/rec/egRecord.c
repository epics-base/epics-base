/*
 
*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.
 
Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods.
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.
 
*****************************************************************
                                DISCLAIMER
*****************************************************************
 
NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.
 
*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
*/

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
#include"alarm.h"
#include"dbAccess.h"
#include"dbEvent.h"
#include"dbFldTypes.h"
#include"devSup.h"
#include"errMdef.h"
#include"recSup.h"
#include"module_types.h"

#define GEN_SIZE_OFFSET
#include"egRecord.h"
#undef  GEN_SIZE_OFFSET
#include"egDefs.h"

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

struct rset egRSET={
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
