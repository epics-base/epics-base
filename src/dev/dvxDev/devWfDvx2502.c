/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/dev $Id$ */

/*
	Author:	John Winans
 *      Date:  11/23/92
 *
 * NOTES:
 *  We ignore the RARM field in the record.  The DVX system is ALWAYS 
 *  self-rearming.
 *
 *  We also ignore/do not use the following fields:
 *    RATE, PTSS, and BUSY.
 */

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<dbScan.h>
#include	<module_types.h>
#include	<waveformRecord.h>

static long init_record();
static long get_ioint_info();
static long read_wf();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
        DEVSUPFUN       get_ioint_info;
	DEVSUPFUN	read_wf;
	DEVSUPFUN	special_linconv;
} devWfDvx2502={
	6,
	NULL,
	NULL,
	init_record,
	get_ioint_info,
	read_wf,
	NULL};

/*
 * Init the record by checking the type of I/O device selected,
 * waveform data type, and then read in a waveform by calling the driver.
 */
static long init_record(pwf)
struct waveformRecord	*pwf;
{

  /* wf.inp must be an VME_IO */
  if (pwf->inp.type != VME_IO)
  {
    recGblRecordError(S_db_badField,(void *)pwf,
	"devWfDvx2502 (init_record) Illegal INP field");
    return(S_db_badField);
  }
  /* waveform type MUST be short */
  if (pwf->ftvl != DBF_USHORT)
  {
    recGblRecordError(S_db_badField, (void *)pwf,
	"devWfDvx2502 (init_record) waveform data type must be unsigned short");
    return(S_db_badField);
  }

  /* call driver to do initial read */
  read_wf(pwf);
  return(0);
}

/*
 * Provide access to the IOSCANPVT structure associated with the DVX card
 * number that we are reading data from.
 */
static long get_ioint_info(int cmd, struct waveformRecord *pwf,IOSCANPVT *ppvt)
{
    dvx_getioscanpvt(pwf->inp.value.vmeio.card,ppvt);
    return(0);
}

/*
 * process a waveform record by having the dvx driver fill in the last scanned
 * inputs into the waveform record.
 */
static long read_wf(pwf)
struct waveformRecord	*pwf;
{
  struct vmeio *pvmeio;
  long status;

  pvmeio = (struct vmeio *)&(pwf->inp.value);

  if ((status=dvxReadWf(pvmeio->card, 0, pwf->nelm, pwf->bptr, &(pwf->nord))) == 0)
    return(0);

  /* driver returned an error code for some reason */
  recGblSetSevr(pwf, READ_ALARM, INVALID_ALARM);
  return(2);
}
