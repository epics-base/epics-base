#ifndef DBCOMMONPVT_H
#define DBCOMMONPVT_H

#include <compilerDependencies.h>
#include <dbDefs.h>
#include "epicsTimer.h"
#include "dbCommon.h"

struct epicsThreadOSD;

/** Base internal additional information for every record
 *  Must be 8 byte aligned.
 */
#ifdef _WIN32
#pragma pack(push, 8)
#endif
typedef struct dbCommonPvt {
    struct dbRecordNode *recnode;

    /* Thread which is currently processing this record */
    struct epicsThreadOSD* procThread;

    /* Timer for inactivity timeout (.TOUT field) */
    epicsTimerId inactivityTimeout;

    /* actually followed by:
     * struct dbCommon common;
     */
}
#ifdef __GNUC__
__attribute__ ((aligned (8)))
#endif
dbCommonPvt;
#ifdef _WIN32
#pragma pack(pop)
#endif

static EPICS_ALWAYS_INLINE
dbCommonPvt* dbRec2Pvt(struct dbCommon *prec)
{
    return (dbCommonPvt*)((char*)prec - sizeof(dbCommonPvt));
}

static EPICS_ALWAYS_INLINE
dbCommon* dbPvt2Rec(struct dbCommonPvt *pvt)
{
    return (dbCommon*)&pvt[1];
}

#endif // DBCOMMONPVT_H
