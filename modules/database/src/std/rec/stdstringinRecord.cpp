/*************************************************************************\
* Copyright (c) 2020 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef USE_TYPED_RSET
#  define USE_TYPED_RSET
#endif
#ifndef USE_TYPED_DSET
#  define USE_TYPED_DSET
#endif

#include <dbAccess.h>
#include <recSup.h>
#include <recGbl.h>
#include <dbEvent.h>

// include by stdstringinRecord.h
#include "epicsTypes.h"
#include "link.h"
#include "epicsMutex.h"
#include "ellLib.h"
#include "devSup.h"
#include "epicsTime.h"

#define epicsExportSharedSymbols

#define GEN_SIZE_OFFSET
#include <stdstringinRecord.h>
#undef  GEN_SIZE_OFFSET
#include <epicsExport.h>

extern "C" {
VFieldType vfStdString = {"std::string"};
}

namespace  {

ELLLIST vfList = ELLLIST_INIT;
VFieldTypeNode vfStrNode;

long initialize()
{
    vfStrNode.vtype = &vfStdString;
    ellAdd(&vfList, &vfStrNode.node);
    return 0;
}

long init_record(struct dbCommon *pcommon, int pass)
{
    stdstringinRecord *prec = (stdstringinRecord*)pcommon;
    stdstringindset *pdset = (stdstringindset *)(prec->dset);

    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "stdstringin: init_record");
        return S_dev_noDSET;
    }

    if(pass==0) {
        new (&prec->val) std::string();
        new (&prec->oval) std::string();

        if(pdset->common.init_record) {
            long ret = pdset->common.init_record(pcommon);
            if(ret)
                return ret;

            prec->oval = prec->val;
        }
    }
    return 0;
}

long readValue(stdstringinRecord *prec,
               stdstringindset *pdset)
{
    return pdset->read_stdstringin(prec);
}

void monitor(stdstringinRecord *prec)
{
    int monitor_mask = recGblResetAlarms(prec);

    if (prec->oval != prec->val) {
        monitor_mask |= DBE_VALUE | DBE_LOG;
        prec->oval = prec->val;
    }

    if (monitor_mask) {
        db_post_events(prec, &prec->val, monitor_mask);
        db_post_events(prec, &prec->oval, monitor_mask);
    }
}

long process(struct dbCommon *pcommon)
{
    stdstringinRecord *prec = (stdstringinRecord*)pcommon;
    stdstringindset *pdset = (stdstringindset *)(prec->dset);
    unsigned char    pact=prec->pact;
    long status;

    if( (pdset==NULL) || (pdset->read_stdstringin==NULL) ) {
        prec->pact=TRUE;
        recGblRecordError(S_dev_missingSup,(void *)prec,"read_stdstringin");
        return(S_dev_missingSup);
    }

    status=readValue(prec, pdset); /* read the new value */
    /* check if device support set pact */
    if ( !pact && prec->pact ) return(0);

    prec->pact = TRUE;
    recGblGetTimeStamp(prec);

    /* check event list */
    monitor(prec);
    /* process the forward scan link record */
    recGblFwdLink(prec);

    prec->pact=FALSE;
    return status;
}

long cvt_dbaddr(DBADDR *paddr)
{
    // for both VAL and OVAL

    // we don't allow dbPut() via DBF_CHAR.
    // std::string::c_str() warns of dire consequences following modifications
    paddr->ro = 1;
    // we provide vfield access
    paddr->vfields = &vfList;
    // arbitrary limit
    paddr->no_elements = 128;

    paddr->field_type = paddr->dbr_field_type = DBF_CHAR;
    paddr->field_size = 1;


    return 0;
}

long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    stdstringinRecord *prec = (stdstringinRecord*)paddr->precord;

    *offset = 0;
    if(dbGetFieldIndex(paddr)==stdstringinRecordVAL) {
        paddr->pfield = (void*)prec->val.c_str();
        *no_elements = prec->val.empty() ? 0u : prec->val.size()+1u;
        return 0;

    } else if(dbGetFieldIndex(paddr)==stdstringinRecordOVAL) {
        paddr->pfield = (void*)prec->oval.c_str();
        *no_elements = prec->oval.empty() ? 0u : prec->oval.size()+1u;
        return 0;
    }
    return S_db_badField;
}

long put_array_info(DBADDR *paddr, long nNew)
{
    return S_db_noMod;
}

long get_vfield(struct dbAddr *paddr, struct VField *p)
{
    stdstringinRecord *prec = (stdstringinRecord*)paddr->precord;

    if(p->vtype==&vfStdString) {
        VString *pstr = (VString*)p;
        if(dbGetFieldIndex(paddr)==stdstringinRecordVAL) {
            pstr->value = prec->val;
            return 0;
        } else if(dbGetFieldIndex(paddr)==stdstringinRecordOVAL) {
            pstr->value = prec->oval;
            return 0;
        }
    }
    return S_db_badChoice;
}

long put_vfield(struct dbAddr *paddr, const struct VField *p)
{
    stdstringinRecord *prec = (stdstringinRecord*)paddr->precord;

    if(p->vtype==&vfStdString) {
        const VString *pstr = (const VString*)p;
        if(dbGetFieldIndex(paddr)==stdstringinRecordVAL) {
            prec->val = pstr->value;
            return 0;
        } else if(dbGetFieldIndex(paddr)==stdstringinRecordOVAL) {
            prec->oval = pstr->value;
            return 0;
        }
    }
    return S_db_badChoice;
}

#define report NULL
#define special NULL
#define get_value NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset stdstringinRSET={
    RSETNUMBER,
    report,
    initialize,
    init_record,
    process,
    special,
    get_value,
    cvt_dbaddr,
    get_array_info,
    put_array_info,
    get_units,
    get_precision,
    get_enum_str,
    get_enum_strs,
    put_enum_str,
    get_graphic_double,
    get_control_double,
    get_alarm_double,
    get_vfield,
    put_vfield,
};

} // namespace

extern "C" {
epicsExportAddress(rset,stdstringinRSET);
}
