
#ifndef EPICS_ERDEFS_H
#define EPICS_ERDEFS_H

typedef struct ErDsetStruct {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       proc;
} ErDsetStruct;
#endif
