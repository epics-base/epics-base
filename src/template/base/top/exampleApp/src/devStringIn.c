/* devStringIn.c */
/* Example device support module for stringinRecord */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "recGbl.h"
#include "alarm.h"
#include "recSup.h"
#include "devSup.h"
#include "stringinRecord.h"

#include "mymodversion.h"


static long init_record(stringinRecord *prec)
{
    return 0; /* no initialization needed */
}

static long read_si(stringinRecord *prec)
{
    size_t N = strlen(MYMODVERSION);
    if(N<MAX_STRING_SIZE-1) {
        strcpy(prec->val, MYMODVERSION);
    } else {
        /* Not enough space, set an alarm */
        (void)recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
    }
    return 0;
}

#include "epicsExport.h"

static struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_si;
}devSiMyModVersion={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_si,
};
epicsExportAddress(dset,devSiMyModVersion);
