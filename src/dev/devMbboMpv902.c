/* devMbboMpv902.c */
/* share/src/dev $Id$ */

/* devMbboMpv902.c - Device Support Routines	*/
/* Burr Brown MPV 902 				*/


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


/* Create the dset for devAiMbboMpv902 */
long init_record();
long write_mbbo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
}devMbboMpv902={
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
	pmbbo->mask <<= pmbbo->shft;
	status = bo_read(pvmeio->card,pmbbo->mask,&value,BB902);
	if(status==0) pmbbo->rbv = pmbbo->rval = value;
	break;
    default :
	strcpy(message,pmbbo->name);
	strcat(message,": devMbboMpv902 (init_record) Illegal OUT field");
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

	status = bo_driver(pvmeio->card,pmbbo->rval,pmbbo->mask,BB902);
	if(status==0) {
		status = bo_read(pvmeio->card,pmbbo->mask,&value,BB902);
		if(status==0) pmbbo->rbv = value;
		else if(pmbbo->nsev<MAJOR_ALARM ) {
			pmbbo->nsta = READ_ALARM;
			pmbbo->nsev = MAJOR_ALARM;
		}
	} else {
		if(pmbbo->nsev<MAJOR_ALARM ) {
			pmbbo->nsta = WRITE_ALARM;
			pmbbo->nsev = MAJOR_ALARM;
		}
	}
	return(status);
}
