/* devXxxSoft.c */
/* Example device support module */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "cvtTable.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "xxxRecord.h"
#include "epicsExport.h"

/*Create the dset for devXxxSoft */
static long init_record();
static long read_xxx();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_xxx;
}devXxxSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_xxx,
};
epicsExportAddress(dset,devXxxSoft);


static long init_record(pxxx)
    struct xxxRecord	*pxxx;
{
    if(recGblInitConstantLink(&pxxx->inp,DBF_DOUBLE,&pxxx->val))
         pxxx->udf = FALSE;
    return(0);
}

static long read_xxx(pxxx)
    struct xxxRecord	*pxxx;
{
    long status;

    status = dbGetLink(&(pxxx->inp),DBF_DOUBLE, &(pxxx->val),0,0);
    /*If return was succesful then set undefined false*/
    if(!status) pxxx->udf = FALSE;
    return(0);
}
