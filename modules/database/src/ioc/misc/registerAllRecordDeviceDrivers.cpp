/*************************************************************************\
* Copyright (c) 2020 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <exception>
#include <string>
#include <set>
#include <map>

#include <string.h>

#define EPICS_PRIVATE_API

#include <iocsh.h>

#include <epicsStdio.h>
#include <epicsFindSymbol.h>
#include <registryRecordType.h>
#include <registryDeviceSupport.h>
#include <registryDriverSupport.h>
#include <iocshRegisterCommon.h>
#include <registryCommon.h>
#include <dbAccess.h>
#include <dbBase.h>
#include <dbStaticLib.h>

namespace {

struct compareLoc {
    bool operator()(const recordTypeLocation& lhs, const recordTypeLocation& rhs) const
    {
        if(lhs.prset<rhs.prset)
            return true;
        else if(lhs.prset>rhs.prset)
            return false;
        return lhs.sizeOffset<rhs.sizeOffset;
    }
};

// storage which will be referenced by pdbbase.  Must never be free'd
std::set<std::string> names;
std::set<recordTypeLocation, compareLoc> reclocs;

template<typename T, typename Y>
const T& intern(std::set<T,Y>& coll, const T& val)
{
    return *coll.insert(val).first;
}

std::set<void*> registrarsRun;

// gcc circa 4.4 doesn't like iocshVarDef[2] as mapped_type
struct varDef {
    iocshVarDef def[2];
};
typedef std::map<std::string, varDef> vardefs_t;
vardefs_t vardefs;

template<typename T>
T lookupAs(const char* a, const char* b =0, const char* c =0, const char* d =0)
{
    std::string name(a);
    if(b)
        name += b;
    if(c)
        name += c;
    if(d)
        name += d;

    T ret = (T)epicsFindSymbol(name.c_str());
    if(!ret) {
        fprintf(stderr, "Unable to find symbol '%s' : %s\n", name.c_str(), epicsLoadError());
    // all pvar_* are pointers to the exported object
    } else if(ret && !*ret) {
        fprintf(stderr, "symbol '%s' holds NULL\n", name.c_str());
    }
    return ret;
}

} // namespace

void runRegistrarOnce(void (*reg_func)(void))
{
    if(registrarsRun.find((void*)reg_func)!=registrarsRun.end())
        return;

    registrarsRun.insert((void*)reg_func);

    reg_func();
}

void clearRegistrarOnce()
{
    registrarsRun.clear();
}

long
registerAllRecordDeviceDrivers(DBBASE *pdbbase)
{
    DBENTRY entry;

    if(!pdbbase) {
        fprintf(stderr, "Must call dbLoadRecords() before registerAllRecordDeviceDrivers(pdbbase)\n");
        return 1;
    }

    dbInitEntry(pdbbase, &entry);
    try {
        // for each recordType
        for(long status=dbFirstRecordType(&entry); !status; status=dbNextRecordType(&entry)) {
            dbRecordType *rtype = entry.precordType;

            if(!registryRecordTypeFind(rtype->name)) {

                rset** prset = lookupAs<rset**>("pvar_rset_", rtype->name, "RSET");
                computeSizeOffset* sizeOffset = lookupAs<computeSizeOffset*>("pvar_func_", rtype->name, "RecordSizeOffset");

                if(!prset || !*prset || !sizeOffset || !*sizeOffset) {
                    fprintf(stderr, "Unable to find support for record type '%s' : %s\n",
                            rtype->name, epicsLoadError());
                    return 1;
                }

                recordTypeLocation sup;
                sup.prset = *prset;
                sup.sizeOffset = *sizeOffset;

                const char *cname = intern<std::string>(names, rtype->name).c_str();
                const recordTypeLocation* csup = &intern<recordTypeLocation>(reclocs, sup);
                registerRecordTypes(pdbbase, 1, &cname, csup);
            }

            // for each device support for this recordType
            for(ELLNODE *cur = ellFirst(&rtype->devList); cur; cur = ellNext(cur)) {
                devSup& devsup = *CONTAINER(cur, devSup, node);

                if(registryDeviceSupportFind(devsup.name))
                    continue;

                dset** ptr = lookupAs<dset**>("pvar_dset_", devsup.name);

                if(!ptr || !*ptr) {
                    fprintf(stderr, "Unable to find dset for record type '%s' support '%s' : %s\n",
                            rtype->name, devsup.name, epicsLoadError());
                    return 1;
                }

                const char *cname = intern<std::string>(names, devsup.name).c_str();
                registerDevices(pdbbase, 1, &cname, ptr);
            }
        }

        // for each driver
        for(ELLNODE *cur = ellFirst(&pdbbase->drvList); cur; cur = ellNext(cur)) {
            drvSup& drv = *CONTAINER(cur, drvSup, node);

            if(registryDriverSupportFind(drv.name))
                continue;

            drvet** ptr = lookupAs<drvet**>("pvar_drvet_", drv.name);

            if(!ptr || !*ptr) {
                fprintf(stderr, "Unable to find drvet '%s' : %s\n", drv.name, epicsLoadError());
                return 1;
            }

            const char *cname = intern<std::string>(names, drv.name).c_str();
            registerDrivers(pdbbase, 1, &cname, ptr);
        }

        // for each link support
        for(ELLNODE *cur = ellFirst(&pdbbase->linkList); cur; cur = ellNext(cur)) {
            linkSup& lnk = *CONTAINER(cur, linkSup, node);

            jlif** ptr = lookupAs<jlif**>("pvar_jlif_", lnk.jlif_name);

            if(!ptr || !*ptr) {
                fprintf(stderr, "Unable to find link support '%s' : %s\n", lnk.jlif_name, epicsLoadError());
                return 1;
            }

            registerJLinks(pdbbase, 1, ptr);
        }

        // for each function()
        for(ELLNODE *cur = ellFirst(&pdbbase->functionList); cur; cur = ellNext(cur)) {
            dbText& reg = *CONTAINER(cur, dbText, node);

            typedef void(*registrar)(void);
            registrar* ptr = lookupAs<registrar*>("pvar_func_register_func_", reg.text);

            if(!ptr || !*ptr) {
                fprintf(stderr, "Unable to find function '%s' : %s\n", reg.text, epicsLoadError());
                return 1;
            }

            runRegistrarOnce(*ptr);
        }

        // for each registrar()
        for(ELLNODE *cur = ellFirst(&pdbbase->registrarList); cur; cur = ellNext(cur)) {
            dbText& reg = *CONTAINER(cur, dbText, node);

            typedef void(*registrar)(void);
            registrar* ptr = lookupAs<registrar*>("pvar_func_", reg.text);

            if(!ptr || !*ptr) {
                fprintf(stderr, "Unable to find registrar '%s' : %s\n", reg.text, epicsLoadError());
                return 1;
            }

            runRegistrarOnce(*ptr);
        }

        // for each iocsh variable
        for(ELLNODE *cur = ellFirst(&pdbbase->variableList); cur; cur = ellNext(cur)) {
            dbVariableDef& var = *CONTAINER(cur, dbVariableDef, node);

            if(iocshFindVariable(var.name))
                continue;

            void** ptr = lookupAs<void**>("pvar_", var.type, "_", var.name);

            if(!ptr || !*ptr) {
                fprintf(stderr, "Unable to find variable '%s' : %s\n", var.name, epicsLoadError());
                return 1;
            }

            vardefs_t::mapped_type& vdef = vardefs[var.name];
            vdef.def[0].name = intern<std::string>(names, var.name).c_str();
            vdef.def[0].pval = *ptr;
            if(strcmp(var.type, "double")==0) {
                vdef.def[0].type = iocshArgDouble;
            } else if(strcmp(var.type, "int")==0) {
                vdef.def[0].type = iocshArgInt;
            } else {
                fprintf(stderr, "Unsupported type %s of variable '%s' : %s\n", var.type, var.name, epicsLoadError());
                return 1;
            }
            vdef.def[1].name = 0;
            iocshRegisterVariable(vdef.def);
        }

        dbFinishEntry(&entry);
        return 0;

    } catch(std::exception& e) {
        dbFinishEntry(&entry);
        fprintf(stderr, "Error: %s\n", e.what());
        return 2;
    }
}
