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
static long masks[] = {
        0x00000000,
        0x00000001,     0x00000003,     0x00000007,     0x0000000f,
        0x0000001f,     0x0000003f,     0x0000007f,     0x000000ff,
        0x000001ff,     0x000003ff,     0x000007ff,     0x00000fff,
        0x00001fff,     0x00003fff,     0x00007fff,     0x0000ffff,
        0x0001ffff,     0x0003ffff,     0x0007ffff,     0x000fffff,
        0x001fffff,     0x003fffff,     0x007fffff,     0x00ffffff,
        0x01ffffff,     0x03ffffff,     0x07ffffff,     0x0fffffff,
        0x1fffffff,     0x3fffffff,     0x7fffffff,     0xffffffff
};


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
	pmbbo->mask = masks[pmbbo->nobt]<<pvmeio->signal;
	status = bo_read(pvmeio->card,pmbbo->mask,&value,BB902);
	if(status==0) pmbbo->rval = value>> pvmeio->signal;
	break;
    default :
	strcpy(message,pmbbo->name);
	strcat(message,": devMbboMpv902 (init_record) Illegal INP field");
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
	if(pmbbo->sdef) {
		unsigned long *pvalues = &(pmbbo->zrvl);

		if(pmbbo->val<0 || pmbbo->val>15) {
			if(pmbbo->nsev<MAJOR_ALARM ) {
				pmbbo->nsta = SOFT_ALARM;
				pmbbo->nsev = MAJOR_ALARM;
			}
			return(0);
		}
		value=pvalues[pmbbo->val];
		pmbbo->rval = value<<pvmeio->signal;
	} else pmbbo->rval = ((unsigned long)(pmbbo->val))<<pvmeio->signal;

	status = bo_driver(pvmeio->card,pmbbo->rval,pmbbo->mask,BB902);
	if(status==0) {
		status = bo_read(pvmeio->card,pmbbo->mask,&value,BB902);
		if(status==0) pmbbo->rval = value>> pvmeio->signal;
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
