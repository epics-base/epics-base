/* share/src/dev $Id$ */

/*
	Author:	John Winans
 *      Date:  11/23/92
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 *	.01	92-11-24	jrw	Added some doc and the NORD parm to 
 *					the driver call.
 * 	...
 *
 * NOTES:
 *  We ignore the RARM field in the record.  The DVX system is ALWAYS 
 *  self-rearming.
 *
 *  We also ignore/do not use the following fields:
 *    RATE, PTSS, and BUSY.
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
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

long init_record();
long get_ioint_info();
long read_wf();

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
  unsigned short value;
  long status;

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
  unsigned short value;
  struct vmeio *pvmeio;
  long status;

  pvmeio = (struct vmeio *)&(pwf->inp.value);

  if ((status=dvxReadWf(pvmeio->card, 0, pwf->nelm, pwf->bptr, &(pwf->nord))) == 0)
    return(0);

  /* driver returned an error code for some reason */
  recGblSetSevr(pwf, READ_ALARM, INVALID_ALARM);
  return(2);
}
