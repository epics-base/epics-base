/*
 * Stringin device support for EPICS time stamps
 *
 * $Id$
 */

#include <stdio.h>
#include <epicsTime.h>
#include <stringinRecord.h>
#include <alarm.h>
#include <devSup.h>
#include <recGbl.h>
#include <epicsExport.h>

static long
read_stringin (struct stringinRecord *psi)
{
    int l;

    recGblGetTimeStamp(psi);
    l = epicsTimeToStrftime(psi->val, sizeof psi->val, psi->inp.value.instio.string, &psi->time);
    if (l >= sizeof psi->val) {
        psi->udf = 1;
        recGblSetSevr(psi,UDF_ALARM,INVALID_ALARM);
        return -1;
    }
    psi->udf = 0;
    return 0;
}

struct {
    long        number;
    DEVSUPFUN   dev_report;
    DEVSUPFUN   init;
    DEVSUPFUN   init_record; /*returns: (0,2)=>(success,success no convert)*/
    DEVSUPFUN   get_ioint_info;
    DEVSUPFUN   read_stringin;/*(0)=>(success ) */
} devTimestampSI = {
    6,
    NULL,
    NULL,
    NULL,
    NULL,
    read_stringin
};
epicsExportAddress(dset,devTimestampSI);
