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
 * 	...
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

static long init_record(pwf)
struct waveformRecord	*pwf;
{
  unsigned short value;
  long status;

printf("devWfDvx2502: init routine entered\n");

  /* wf.inp must be an VME_IO */
  if (pwf->inp.type != VME_IO)
  {
    recGblRecordError(S_db_badField,(void *)pwf,
	"devWfDvx2502 (init_record) Illegal INP field");
    return(S_db_badField);
  }
  /* waveform type MUST be short */
  if (pwf->ftvl != DBF_SHORT)
  {
    recGblRecordError(S_db_badField, (void *)pwf,
	"devWfDvx2502 (init_record) waveform data type must be short");
    return(S_db_badField);
  }

  /* call driver to do initial read */
  read_wf(pwf);
  return(0);
}

static long get_ioint_info(int cmd, struct waveformRecord *pwf,IOSCANPVT *ppvt)
{
    dvx_getioscanpvt(pwf->inp.value.vmeio.card,ppvt);
    return(0);
}

static long read_wf(pwf)
struct waveformRecord	*pwf;
{
  unsigned short value;
  struct vmeio *pvmeio;
  long status;

printf("devWfDvx2502: read routine entered\n");

  pvmeio = (struct vmeio *)&(pwf->inp.value);

  if ((status=dvxReadWf(pvmeio->card, 0, pwf->nelm, pwf->bptr)) == 0)
  {
    pwf->nord = pwf->nelm;
    return(0);
  }

  pwf->nord = 0;
  recGblSetSevr(pwf, READ_ALARM, INVALID_ALARM);
  return(2);
}
