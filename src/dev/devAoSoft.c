/* devAoSoft.c */
/* share/src/dev $Id$ */

/* Device Support Routines for soft Analog Output Records*/


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<devSup.h>
#include	<link.h>
#include	<special.h>
#include	<aoRecord.h>

/* Create the dset for devAoSoft */
long init_record();
long write_ao();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}devAoSoft={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_ao,
	NULL};


static long init_record(pao)
    struct aoRecord	*pao;
{
    char message[100];

    /* ao.out must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pao->out.type) {
    case (CONSTANT) :
	pao->val = pao->out.value.value;
	break;
    case (PV_LINK) :
	break;
    case (DB_LINK) :
	break;
    case (CA_LINK) :
	break;
    default :
	strcpy(message,pao->name);
	strcat(message,": devAoSoft (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    /* Make sure record processing routine does not perform any conversion*/
    pao->linr=0;
    return(0);
}

static long write_ao(pao)
    struct aoRecord	*pao;
{
    char message[100];

    /* ao.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pao->out.type) {
    case (CONSTANT) :
	break;
    case (DB_LINK) :
	(void)dbPutLink(&pao->out.value.db_link,pao,DBR_FLOAT,
		&pao->val,1L);
	break;
    case (CA_LINK) :
	break;
    default :
	if(pao->nsev<MAJOR_ALARM) {
		pao->nsev = MAJOR_ALARM;
		pao->nsta = SOFT_ALARM;
		if(pao->stat!=SOFT_ALARM) {
			strcpy(message,pao->name);
			strcat(message,": devAoSoft (write_ao) Illegal INP field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
