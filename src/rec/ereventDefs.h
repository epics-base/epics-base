#ifndef EPICS_EREVENTDEFS_H
#define EPICS_EREVENTDEFS_H

typedef struct EreventDsetStruct {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       initRec;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       proc;
} EreventDsetStruct;
#endif
