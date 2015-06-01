/* devGenVersion.c */
/* Example device support providing the module version string as a charactor array (long string) */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "recGbl.h"
#include "alarm.h"
#include "recSup.h"
#include "devSup.h"
#include "menuFtype.h"
#include "waveformRecord.h"

#include "_APPNAME_Version.h"

/* must be last include */
#include "epicsExport.h"

static long read_wf(waveformRecord *prec)
{
    size_t N = strlen(_APPNAME_VERSION)+1;
    char *buf = prec->bptr;

    if(prec->ftvl!=menuFtypeCHAR) {
        (void)recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return 0;
    }

    if(N>=prec->nelm)
        N = prec->nelm;
    prec->nord = N;

    memcpy(buf, _APPNAME_VERSION, N);
    buf[prec->nelm-1] = '\0';

    return 0;
}

static struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_si;
}devWfMyModVersion={
	5,
	NULL,
	NULL,
	NULL,
	NULL,
	read_wf,
};
epicsExportAddress(dset,devWfMyModVersion);
