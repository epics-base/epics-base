/* devBiMpv910.c */
/* share/src/dev $Id$ */

/* devBiMpv910.c - Device Support Routines for  Burr Brown MPV 910  Binary input */


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


/* Create the dset for devAiBiMpv910 */
long init_record();
long read_bi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;
}devBiMpv910={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_bi};
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


static long init_record(pbi)
    struct biRecord	*pbi;
{
    char message[100];
    short value;
    struct vmeio *pvmeio;

    /* bi.inp must be an VME_IO */
    switch (pbi->inp.type) {
    case (VME_IO) :
	break;
    default :
	strcpy(message,pbi->name);
	strcat(message,": devBiMpv910 (init_record) Illegal INP field");
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
	status = bi_driver(pvmeio->card,masks[pvmeio->signal],BB910,&value);
	if(status==0) {
		pbi->rval = value;
		if(value == 0) pbi->val = 0;
		else pbi->val = 1;
	} else {
		if(pbi->nsev<MAJOR_ALARM ) {
			pbi->nsta = READ_ALARM;
			pbi->nsev = MAJOR_ALARM;
		}
	}
	return(status);
}
