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
static long masks[] = {
        0x00000000,
        0x00000001,     0x00000003,     0x00000007,     0x0000000f,
        0x0000001f,     0x0000003f,     0x0000007f,     0x000000ff,
        0x000001ff,     0x000003ff,     0x000007ff,     0x00000fff,
        0x00001fff,     0x00003fff,     0x00007fff,     0x0000ffff,
        0x0001ffff,     0x0003ffff,     0x0007ffff,     0x000fffff,
        0x001fffff,     0x003fffff,     0x007fffff,     0x00ffffff,
        0x01ffffff,     0x03ffffff,     0x07ffffff,     0x0fffffff,
        0x1fffffff,     0x3fffffff,     0x7fffffff,     0xffffffff
};


static long init_record(pmbbi)
    struct mbbiRecord	*pmbbi;
{
    char message[100];
    struct vmeio *pvmeio;

    /* mbbi.inp must be an VME_IO */
    switch (pmbbi->inp.type) {
    case (VME_IO) :
        pmbbi->mask = masks[pmbbi->nobt]<<pmbbi->inp.value.vmeio.signal;
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
	status = bi_driver(pvmeio->card,masks[pvmeio->signal],XY210,&value);
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
