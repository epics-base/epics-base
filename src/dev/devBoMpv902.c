/* devBoMpv902.c */
/* share/src/dev $Id$ */

/* devBoMpv902.c - Device Support Routines for  Burr Brown MPV 902  Binary output */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<boRecord.h>


/* Create the dset for devAiBoMpv902 */
long init_record();
long write_bo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
}devBoMpv902={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_bo};

static long init_record(pbo)
    struct boRecord	*pbo;
{
    char message[100];
    int value,status;
    struct vmeio *pvmeio;

    /* bo.out must be an VME_IO */
    switch (pbo->out.type) {
    case (VME_IO) :
        pvmeio = (struct vmeio *)(&pbo->out.value);
	pbo->mask = 1;
	pbo->mask <<= pvmeio->signal;
        /* read the value via bo driver */
        status = bo_read(pvmeio->card,pbo->mask,&value,BB902);
	if(status == 0) {
		pbo->rval = value;
	} else if(status == -1) {
		strcpy(message,pbo->name);
		strcat(message,": devBoMpv902 (init_record) card does not exist");
		errMessage(S_db_badField,message);
		return(S_db_badField);
	} else {
		strcpy(message,pbo->name);
		strcat(message,": devBoMpv902 (init_record) illegal device");
		errMessage(S_db_badField,message);
		return(S_db_badField);
	}
	break;
    default :
	strcpy(message,pbo->name);
	strcat(message,": devBoMpv902 (init_record) Illegal OUT field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long write_bo(pbo)
    struct boRecord	*pbo;
{
	struct vmeio *pvmeio;
	int	    status;

	
	pvmeio = (struct vmeio *)&(pbo->out.value);
	status = bo_driver(pvmeio->card,pbo->rval,pbo->mask,BB902);
	if(status!=0) {
		if(pbo->nsev<VALID_ALARM ) {
			pbo->nsta = WRITE_ALARM;
			pbo->nsev = VALID_ALARM;
		}
	}
	return(status);
}
