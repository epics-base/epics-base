/* devAiSoftRaw.c */
/* share/src/dev $Id$ */

/* devAiSoftRaw.c - Device Support Routines for soft Analog Input Records*/


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<devSup.h>
#include	<link.h>
#include	<aiRecord.h>

/* Create the dset for devAiSoftRaw */
long init_record();
long read_ai();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
}devAiSoftRaw={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_ai,
	NULL};


static long init_record(pai)
    struct aiRecord	*pai;
{
    char message[100];

    /* ai.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	pai->rval = pai->inp.value.value;
	break;
    case (PV_LINK) :
	break;
    case (DB_LINK) :
	break;
    case (CA_LINK) :
	break;
    default :
	strcpy(message,pai->name);
	strcat(message,": devAiSoftRaw (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_ai(pai)
    struct aiRecord	*pai;
{
    char message[100];
    long status,options,nRequest;

    /* ai.inp must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	break;
    case (DB_LINK) :
	options=0;
	nRequest=1;
	(void)dbGetLink(&(pai->inp.value.db_link),pai,DBR_LONG,
		&(pai->rval),&options,&nRequest);
	break;
    case (CA_LINK) :
	break;
    default :
	if(pai->nsev<MAJOR_ALARM) {
		pai->nsev = MAJOR_ALARM;
		pai->nsta = SOFT_ALARM;
		if(pai->stat!=SOFT_ALARM) {
			strcpy(message,pai->name);
			strcat(message,": devAiSoftRaw (read_ai) Illegal INP field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(2); /*don't convert*/
}
