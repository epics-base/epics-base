/* devBiSoft.c */
/* share/src/dev $Id$ */

/* devBiSoft.c - Device Support Routines for  Soft Binary Input*/


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<biRecord.h>


/* Create the dset for devBiSoft */
long init_record();
long read_bi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;
}devBiSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_bi};


static long init_record(pbi)
    struct biRecord	*pbi;
{
    char message[100];

    /* bi.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pbi->inp.type) {
    case (CONSTANT) :
        pbi->val = pbi->inp.value.value;
        break;
    case (PV_LINK) :
        break;
    case (DB_LINK) :
        break;
    case (CA_LINK) :
        break;
    default :
	strcpy(message,pbi->name);
	strcat(message,": devBiSoft (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_bi(pbi)
    struct biRecord	*pbi;
{
    char message[100];
    long status,options,nRequest;
    unsigned short val;

    /* bi.inp must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pbi->inp.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        options=0;
        nRequest=1;
        status = dbGetLink(&(pbi->inp.value.db_link),pbi,DBR_USHORT,
                &val,&options,&nRequest);
        if(status!=0) {
                if(pbi->nsev<VALID_ALARM) {
                        pbi->nsev = VALID_ALARM;
                        pbi->nsta = LINK_ALARM;
                }
		return(2);
        }
        break;
    case (CA_LINK) :
        break;
    default :
        if(pbi->nsev<MAJOR_ALARM) {
                pbi->nsev = MAJOR_ALARM;
                pbi->nsta = SOFT_ALARM;
                if(pbi->stat!=SOFT_ALARM) {
                        strcpy(message,pbi->name);
                        strcat(message,": devBiSoft (read_bi) Illegal INP field");
                        errMessage(S_db_badField,message);
                }
        }
    }
    return(2);
}
