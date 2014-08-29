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

/* must be last include */
#include "epicsExport.h"

static long read_si(stringinRecord *prec)
{
    size_t N = strlen(MODULEVERSION);
    if(N<sizeof(prec->val)) {
        strcpy(prec->val, MODULEVERSION);
    } else {
        /* Not enough space, so signal an alarm */
        (void)recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
    }
    return 0;
}

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
	NULL,
	NULL,
	read_si,
};
epicsExportAddress(dset,devSiMyModVersion);
