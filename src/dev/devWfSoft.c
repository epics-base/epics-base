/* devWfSoft.c */
/* share/src/dev $Id$ */

/* devWfSoft.c - Device Support Routines for soft Waveform Records*/


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<devSup.h>
#include	<link.h>
#include	<waveformRecord.h>

/* Create the dset for devWfSoft */
long init_record();
long read_wf();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_wf;
}devWfSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_wf};


static long init_record(pwf)
    struct waveformRecord	*pwf;
{
    char message[100];

    /* wf.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	pwf->nord = 0;
	break;
    case (PV_LINK) :
	break;
    case (DB_LINK) :
	break;
    case (CA_LINK) :
	break;
    default :
	strcpy(message,pwf->name);
	strcat(message,": devWfSoft (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_wf(pwf)
    struct waveformRecord	*pwf;
{
    char message[100];
    long status,options,nRequest;

    /* wf.inp must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	break;
    case (DB_LINK) :
	options=0;
	nRequest=pwf->nelm;
	(void)dbGetLink(&(pwf->inp.value.db_link),pwf,pwf->ftvl,
		pwf->bptr,&options,&nRequest);
	pwf->nord = nRequest;
	break;
    case (CA_LINK) :
	break;
    default :
	if(pwf->nsev<MAJOR_ALARM) {
		pwf->nsev = MAJOR_ALARM;
		pwf->nsta = SOFT_ALARM;
		if(pwf->stat!=SOFT_ALARM) {
			strcpy(message,pwf->name);
			strcat(message,": devWfSoft (read_wf) Illegal INP field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
