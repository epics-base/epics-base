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
long init_record();
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
	init_record,
	NULL,
	write_bo};


static long init_record(pbo)
    struct boRecord	*pbo;
{
    char message[100];
    /* bo.out must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pbo->out.type) {
    case (CONSTANT) :
        pbo->val = pbo->out.value.value;
        break;
    case (PV_LINK) :
        break;
    case (DB_LINK) :
        break;
    case (CA_LINK) :
        break;
    default :
	strcpy(message,pbo->name);
	strcat(message,": devBoSoft (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long write_bo(pbo)
    struct boRecord	*pbo;
{
    char message[100];

    /* bo.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pbo->out.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        (void)dbPutLink(&pbo->out.value.db_link,pbo,DBR_SHORT,&pbo->val,1L);
        break;
    case (CA_LINK) :
        break;
    default :
        if(pbo->nsev<MAJOR_ALARM) {
                pbo->nsev = MAJOR_ALARM;
                pbo->nsta = SOFT_ALARM;
                if(pbo->stat!=SOFT_ALARM) {
                        strcpy(message,pbo->name);
                        strcat(message,": devBoSoft (write_ai) Illegal INP field");
                        errMessage(S_db_badField,message);
                }
        }
    }
    return(0);
}
