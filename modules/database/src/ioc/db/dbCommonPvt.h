#ifndef DBCOMMONPVT_H
#define DBCOMMONPVT_H

#include <compilerDependencies.h>
#include <dbDefs.h>
#include "dbCommon.h"

/** Base internal additional information for every record
 */
typedef struct dbCommonPvt {
    struct dbRecordNode *recnode;

    struct dbCommon common;
} dbCommonPvt;

static EPICS_ALWAYS_INLINE
dbCommonPvt* dbRec2Pvt(struct dbCommon *prec)
{
    return CONTAINER(prec, dbCommonPvt, common);
}

#endif // DBCOMMONPVT_H
