/* devAiDvx2502.c */
/* share/src/dev $Id$ */

/* devAiDvx2502.c - Device Support Routines */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<aiRecord.h>

long init_record();
long get_ioint_info();
long read_ai();
long special_linconv();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
        DEVSUPFUN       get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
} devAiDvx2502={
	6,
	NULL,
	NULL,
	init_record,
	get_ioint_info,
	read_ai,
	special_linconv};


static long init_record(pai)
    struct aiRecord	*pai;
{
    char message[100];
    unsigned short value;
    struct vmeio *pvmeio;
    long status;

    /* ai.inp must be an VME_IO */
    switch (pai->inp.type) {
    case (VME_IO) :
	break;
    default :
	strcpy(message,pai->name);
	strcat(message,": devAiDvx2502 (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/65535.0;

    /* call driver so that it configures card */
    pvmeio = (struct vmeio *)&(pai->inp.value);
    if(status=ai_driver(pvmeio->card,pvmeio->signal,DVX2502,&value)) {
	strcpy(message,pai->name);
	strcat(message,": devAiDvx2502 (init_record) ai_driver error");
	errMessage(status,message);
	return(status);
    }
    return(0);
}

static long get_ioint_info(pai,io_type,card_type,card_number)
    struct aiRecord     *pai;
    short               *io_type;
    short               *card_type;
    short               *card_number;
{
    if(pai->inp.type != VME_IO) return(S_dev_badInpType);
    *io_type = IO_AI;
    *card_type = DVX2502;
    *card_number = pai->inp.value.vmeio.card;
    return(0);
}

static long read_ai(pai)
    struct aiRecord	*pai;
{
	unsigned short value;
	struct vmeio *pvmeio;
	long status;

	
	pvmeio = (struct vmeio *)&(pai->inp.value);
	if(status=ai_driver(pvmeio->card,pvmeio->signal,DVX2502,&value))
		return(status);
	*((unsigned short*)(&pai->rval))=value;
	if(status==0 || status==-2) pai->rval = value;
        if(status==-1) {
		if(pai->nsev<VALID_ALARM ) {
                	pai->nsta = READ_ALARM;
                	pai->nsev = VALID_ALARM;
		}
		status=2; /*don't convert*/
        }else if(status==-2) {
                status=0;
                if(pai->nsev<VALID_ALARM ) {
                        pai->nsta = HW_LIMIT_ALARM;
                        pai->nsev = VALID_ALARM;
                }
        }
	return(status);
}

static long special_linconv(pai,after)
    struct aiRecord	*pai;
    int after;
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/65535.0;
    return(0);
}
