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
 *      Author:		John Winans
 *      Date:           8/27/93
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbEvent.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>

#include	<ereventDefs.h>
#define GEN_SIZE_OFFSET
#include	<ereventRecord.h>
#undef  GEN_SIZE_OFFSET

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

struct rset ereventRSET={
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
	get_alarm_double };



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
