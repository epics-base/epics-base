/* devMbboXVme220.c */
/* share/src/dev $Id$ */

/* devMbboXVme220.c - Device Support Routines	*/
/* XYcom 32 bit binary output			*/


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<mbboRecord.h>


/* Create the dset for devAiMbboXVme220 */
long init_record();
long write_mbbo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
}devMbboXVme220={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_mbbo};

static long init_record(pmbbo)
    struct mbboRecord	*pmbbo;
{
    char message[100];
    unsigned long value;
    struct vmeio *pvmeio;
    int		status;

    /* mbbo.out must be an VME_IO */
    switch (pmbbo->out.type) {
    case (VME_IO) :
	pvmeio = &(pmbbo->out.value.vmeio);
	pmbbo->shft = pvmeio->signal;
	pmbbo->mask = pmbbo->shft;
	status = bo_read(pvmeio->card,pmbbo->mask,&value,XY220);
	if(status==0) pmbbo->rbv = pmbbo->rval = value;
	break;
    default :
	strcpy(message,pmbbo->name);
	strcat(message,": devMbboXVme220 (init_record) Illegal OUT field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long write_mbbo(pmbbo)
    struct mbboRecord	*pmbbo;
{
	struct vmeio *pvmeio;
	int	    status;
	unsigned long value;

	
	pvmeio = &(pmbbo->out.value.vmeio);
	status = bo_driver(pvmeio->card,pmbbo->rval,pmbbo->mask,XY220);
	if(status==0) {
		status = bo_read(pvmeio->card,pmbbo->mask,&value,XY220);
		if(status==0) pmbbo->rbv = value;
		else if(pmbbo->nsev<VALID_ALARM ) {
			pmbbo->nsta = READ_ALARM;
			pmbbo->nsev = VALID_ALARM;
		}
	} else {
		if(pmbbo->nsev<VALID_ALARM ) {
			pmbbo->nsta = WRITE_ALARM;
			pmbbo->nsev = VALID_ALARM;
		}
	}
	return(status);
}
