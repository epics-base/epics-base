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
	NULL,
	NULL,
	write_ao,
	NULL};


static long write_ao(pao)
    struct aoRecord	*pao;
{
    char message[100];
    long status;

    /* ao.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pao->out.type) {
    case (CONSTANT) :
	break;
    case (DB_LINK) :
	status = dbPutLink(&pao->out.value.db_link,pao,DBR_DOUBLE,
		&pao->oval,1L);
        if(status!=0) {
                if(pao->nsev<MAJOR_ALARM) {
                        pao->nsev = MAJOR_ALARM;
                        pao->nsta = LINK_ALARM;
                }
        }
	break;
    case (CA_LINK) :
	break;
    default :
	if(pao->nsev<MAJOR_ALARM) {
		pao->nsev = MAJOR_ALARM;
		pao->nsta = SOFT_ALARM;
		if(pao->stat!=SOFT_ALARM) {
			strcpy(message,pao->name);
			strcat(message,": devAoSoft (write_ao) Illegal OUT field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
