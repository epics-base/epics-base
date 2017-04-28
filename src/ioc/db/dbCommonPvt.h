#ifndef DBCOMMONPVT_H
#define DBCOMMONPVT_H

#include "dbCommon.h"

/** Base internal additional information for every record
 */
typedef struct dbCommonPvt {
    struct dbRecordNode *node;

    struct dbCommon common;
} dbCommonPvt;

#endif // DBCOMMONPVT_H
