/* devBoSoft.c */
/* share/src/dev $Id$ */

/* devBoSoft.c - Device Support Routines for  Soft Binary Output*/


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


/* Create the dset for devBoSoft */
long write_bo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
}devBoSoft={
	5,
	NULL,
	NULL,
	NULL,
	NULL,
	write_bo};


static long write_bo(pbo)
    struct boRecord	*pbo;
{
    char message[100];
    long status;

    /* bo.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pbo->out.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        status = dbPutLink(&pbo->out.value.db_link,pbo,DBR_USHORT,&pbo->val,1L);
        if(status!=0) {
                if(pbo->nsev<VALID_ALARM) {
                        pbo->nsev = VALID_ALARM;
                        pbo->nsta = LINK_ALARM;
                }
        }
        break;
    case (CA_LINK) :
        break;
    default :
        if(pbo->nsev<VALID_ALARM) {
                pbo->nsev = VALID_ALARM;
                pbo->nsta = SOFT_ALARM;
                if(pbo->stat!=SOFT_ALARM) {
                        strcpy(message,pbo->name);
                        strcat(message,": devBoSoft (write_bo) Illegal OUT field");
                        errMessage(S_db_badField,message);
                }
        }
    }
    return(0);
}
