/* devMbboSoft.c */
/* share/src/dev $Id$ */

/* devMbboSoft.c - Device Support Routines for  Soft Multibit Binary Output*/


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


/* Create the dset for devMbboSoft */
long init_record();
long write_mbbo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
}devMbboSoft={
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
    /* mbbo.out must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pmbbo->out.type) {
    case (CONSTANT) :
        pmbbo->val = pmbbo->out.value.value;
        break;
    case (PV_LINK) :
        break;
    case (DB_LINK) :
        break;
    case (CA_LINK) :
        break;
    default :
	strcpy(message,pmbbo->name);
	strcat(message,": devMbboSoft (init_record) Illegal OUT field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long write_mbbo(pmbbo)
    struct mbboRecord	*pmbbo;
{
    char message[100];

    /* mbbo.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pmbbo->out.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        (void)dbPutLink(&pmbbo->out.value.db_link,pmbbo,DBR_ENUM,
                &pmbbo->val,1L);
        break;
    case (CA_LINK) :
        break;
    default :
        if(pmbbo->nsev<MAJOR_ALARM) {
                pmbbo->nsev = MAJOR_ALARM;
                pmbbo->nsta = SOFT_ALARM;
                if(pmbbo->stat!=SOFT_ALARM) {
                        strcpy(message,pmbbo->name);
                        strcat(message,": devMbboSoft (write_mbbo) Illegal INP field");
                        errMessage(S_db_badField,message);
                }
        }
    }
    return(0);
}
