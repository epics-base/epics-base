/* devMbbiXVme210.c */
/* share/src/dev $Id$ */

/* devMbbiXVme210.c - Device Support Routines	*/
/* XYcom 32 bit Multibit binary input		*/


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


/* Create the dset for devAiMbbiXVme210 */
long init_record();
long read_mbbi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;
}devMbbiXVme210={
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

    /* mbbi.inp must be an VME_IO */
    switch (pmbbi->inp.type) {
    case (VME_IO) :
	pmbbi->shft = pmbbi->inp.value.vmeio.signal;
	pmbbi->mask <<= pmbbi->shft;
	break;
    default :
	strcpy(message,pmbbi->name);
	strcat(message,": devMbbiXVme210 (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_mbbi(pmbbi)
    struct mbbiRecord	*pmbbi;
{
	struct vmeio	*pvmeio;
	int		status;
	unsigned long	value;

	
	pvmeio = (struct vmeio *)&(pmbbi->inp.value);
	status = bi_driver(pvmeio->card,pmbbi->mask,XY210,&value);
	if(status==0) {
		pmbbi->rval = value;
	} else {
		if(pmbbi->nsev<MAJOR_ALARM ) {
			pmbbi->nsta = READ_ALARM;
			pmbbi->nsev = MAJOR_ALARM;
		}
	}
	return(status);
}
