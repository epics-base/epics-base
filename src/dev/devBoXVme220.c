/* devBoXVme220.c */
/* share/src/dev $Id$ */

/* devBoXVme220.c - Device Support Routines for XYcom 32 bit binary output*/


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


/* Create the dset for devAiBoXVme220 */
long init_record();
long write_bo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
}devBoXVme220={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_bo};
static long masks[] = {
        0x00000001,0x00000002,0x00000004,0x00000008,
        0x00000010,0x00000020,0x00000040,0x00000080,
        0x00000100,0x00000200,0x00000400,0x00000800,
        0x00001000,0x00002000,0x00004000,0x00008000,
        0x00010000,0x00020000,0x00040000,0x00080000,
        0x00100000,0x00200000,0x00400000,0x00800000,
        0x01000000,0x02000000,0x04000000,0x08000000,
        0x10000000,0x20000000,0x40000000,0x80000000,
};


static long init_record(pbo)
    struct boRecord	*pbo;
{
    char message[100];
    int value,status;
    struct vmeio *pvmeio;

    /* bo.out must be an VME_IO */
    switch (pbo->out.type) {
    case (VME_IO) :
	pvmeio = (struct vmeio *)&(pbo->out.value);
        status = bo_read(pvmeio->card,masks[pvmeio->signal],&value,XY220);
        if(status == 0) {
                pbo->rbv = pbo->val = (value == 0)?0:1;
        } else if(status == -1) {
                strcpy(message,pbo->name);
                strcat(message,": devBoXVme220 (init_record) card does not exist");
                errMessage(S_db_badField,message);
                return(S_db_badField);
        } else {
                strcpy(message,pbo->name);
                strcat(message,": devBoXVme220 (init_record) illegal device");
                errMessage(S_db_badField,message);
                return(S_db_badField);
        }
	break;
    default :
	strcpy(message,pbo->name);
	strcat(message,": devBoXVme220 (init_record) Illegal OUT field");
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
	if(pbo->val == 0) pbo->rval = 0;
	else pbo->rval = masks[pvmeio->signal];
	status = bo_driver(pvmeio->card,pbo->rval,masks[pvmeio->signal],XY220);
	if(status!=0) {
		if(pbo->nsev<MAJOR_ALARM ) {
			pbo->nsta = WRITE_ALARM;
			pbo->nsev = MAJOR_ALARM;
		}
	}
	return(status);
}
