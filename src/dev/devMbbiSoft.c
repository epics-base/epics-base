/* devMbbiSoft.c */
/* share/src/dev @(#)devMbbiSoft.c	1.2     11/30/90 */

/* devMbbiSoft.c - Device Support Routines for  Soft Multibit Binary Input*/


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<mbbiRecord.h>


/* Create the dset for devMbbiSoft */
long init_record();
long read_mbbi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;
}devMbbiSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_mbbi};


static long init_record(pmbbi)
    struct mbbiRecord	*pmbbi;
{
    char message[100];
    /* mbbi.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pmbbi->inp.type) {
    case (CONSTANT) :
        pmbbi->val = pmbbi->inp.value.value;
        break;
    case (PV_LINK) :
        break;
    case (DB_LINK) :
        break;
    case (CA_LINK) :
        break;
    default :
	strcpy(message,pmbbi->name);
	strcat(message,": devMbbiSoft (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_mbbi(pmbbi)
    struct mbbiRecord	*pmbbi;
{
    char message[100];
    long status,options,nRequest;

    /* mbbi.inp must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pmbbi->inp.type) {
    case (CONSTANT) :
	return(2);
        break;
    case (DB_LINK) :
        options=0;
        nRequest=1;
        status = dbGetLink(&(pmbbi->inp.value.db_link),pmbbi,DBR_USHORT,
                &(pmbbi->val),&options,&nRequest);
        if(status!=0) {
                if(pmbbi->nsev<VALID_ALARM) {
                        pmbbi->nsev = VALID_ALARM;
                        pmbbi->nsta = LINK_ALARM;
                }
		return(2);
        }
        break;
    case (CA_LINK) :
        break;
    default :
        if(pmbbi->nsev<MAJOR_ALARM) {
                pmbbi->nsev = MAJOR_ALARM;
                pmbbi->nsta = SOFT_ALARM;
                if(pmbbi->stat!=SOFT_ALARM) {
                        strcpy(message,pmbbi->name);
                        strcat(message,": devMbbiSoft (read_mbbi) Illegal INP field");
                        errMessage(S_db_badField,message);
                }
        }
	return(2);
    }
    return(0);
}
