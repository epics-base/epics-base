/* devAoVmiVme4100.c.c */
/* share/src/dev $Id$ */

/* Device Support Routines for VMI4100 analog output*/


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<devSup.h>
#include	<link.h>
#include	<special.h>
#include	<module_types.h>
#include	<aoRecord.h>

/* The following must match the definition in choiceGbl.ascii */
#define LINEAR 1

/* Create the dset for devAoAoVmiVme4100 */
long init_record();
long write_ao();
long special_linconv();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}devAoVmiVme4100={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_ao,
	special_linconv};

void read_ao();


static long init_record(pao)
    struct aoRecord	*pao;
{
    char message[100];
    short value;
    struct vmeio *pvmeio;

    /* ao.out must be an VME_IO */
    switch (pao->out.type) {
    case (VME_IO) :
	break;
    default :
	strcpy(message,pao->name);
	strcat(message,": devAoVmiVme4100 (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pao->eslo = (pao->eguf -pao->egul)/4095.0;

    /* call driver so that it configures card */
    read_ao(pao);
    return(0);
}

static long write_ao(pao)
    struct aoRecord	*pao;
{
	struct vmeio 	*pvmeio;
	int	    	status;
	unsigned short  value,rbvalue;

	
	pvmeio = (struct vmeio *)&(pao->out.value);
	value = pao->rval;
	status = ao_driver(pvmeio->card,pvmeio->signal,pao->type,&value,&rbvalue);
	if(status==0 || status==-2) pao->rbv = rbvalue;
	if(status==-1) {
		if(pao->nsev<MAJOR_ALARM ) {
			pao->nsta = WRITE_ALARM;
			pao->nsev = MAJOR_ALARM;
		}
		status=2; /*dont convert*/
	}else if(status==-2) {
		status=0;
		if(pao->nsev<MAJOR_ALARM ) {
			pao->nsta = HW_LIMIT_ALARM;
			pao->nsev = MAJOR_ALARM;
		}
	}
	return(status);
}


static long special_linconv(pao,after)
    struct aoRecord	*pao;
    int after;
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pao->eslo = (pao->eguf -pao->egul)/4095.0;
    return(0);
}

static void read_ao(pao)
struct aoRecord      *pao;
{
	int            status;
	unsigned short          value;
	struct vmeio    		*pvmeio = &pao->out.value.vmeio;

	/* get the value from the ao driver */
	ao_read(pvmeio->card,pvmeio->signal,pao->type,&value);
	/* convert raw readback to egu */
	switch (pao->linr){
		case (LINEAR):
			pao->rbv = (value * pao->eslo) + pao->egul;
			break;
		default:
			pao->rbv = value;
	}
	pao->val = pao->rbv;
	return;
}

