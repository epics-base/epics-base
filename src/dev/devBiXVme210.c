/* devBiXVme210.c */
/* share/src/dev $Id$ */

/* devBiXVme210.c - Device Support Routines for XYcom 32 bit binary input*/


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


/* Create the dset for devBiXVme210 */
long init_record();
long read_bi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;
}devBiXVme210={
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
    struct vmeio *pvmeio;

    /* bi.inp must be an VME_IO */
    switch (pbi->inp.type) {
    case (VME_IO) :
	pvmeio = (struct vmeio *)&(pbi->inp.value);
	pbi->mask=1;
	pbi->mask <<= pvmeio->signal;
	break;
    default :
	strcpy(message,pbi->name);
	strcat(message,": devBiXVme210 (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_bi(pbi)
    struct biRecord	*pbi;
{
	struct vmeio *pvmeio;
	int	    status;
	long	    value;

	
	pvmeio = (struct vmeio *)&(pbi->inp.value);
	status = bi_driver(pvmeio->card,pbi->mask,XY210,&value);
	if(status==0) {
		pbi->rval = value;
		return(0);
	} else {
		if(pbi->nsev<VALID_ALARM ) {
			pbi->nsta = READ_ALARM;
			pbi->nsev = VALID_ALARM;
		}
		return(2);
	}
	return(status);
}
