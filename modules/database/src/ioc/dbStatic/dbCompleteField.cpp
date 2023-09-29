#include <string>
#include "CStr.h"

#include <algorithm>
#include <limits>
#include <set>
#include <sstream>

#include <string.h>

#include <epicsStdio.h>
#include <dbAccess.h>
#include "dbStaticPvt.h"
#include "dbTest.h"
using namespace std;

char ** getAllFields(DBENTRY * entry, const char*  pv) {
    DBADDR addr;
    long status = dbNameToAddr(pv, &addr);

    dbFldDes *pdbFldDes = addr.pfldDes;
    dbRecordType *pdbRecordType = addr.pfldDes->pdbRecordType;
    char *pfield_name;

    int idx=1;

    char** result;
    result = (char**)malloc(sizeof(*result)*(2u + pdbRecordType->no_fields));

    for (int n2=0; n2 <=pdbRecordType->no_fields; n2++) {
        pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->sortFldInd[n2]];
        const CStr name(pdbFldDes->name);

        //allocate some new memory to store the pv+.+name...
        char * fullname=(char* )malloc(strlen(pv) + name.size() + 2u);
            
        snprintf(fullname, strlen(pv) + name.size() + 2u, "%s.", pv);
        strncat(fullname, name.p, name.size());

        result[idx] = fullname;
        idx++;
    }
    result[idx] = NULL;
    return result;
}

char** dbCompleteField(const char * pv, const char * init_field)
{
    const CStr word(init_field);
    //first, check the pv actually exists...
    size_t pvlen = strlen(pv);
    
    DBENTRY ent;

    dbInitEntry(pdbbase, &ent);
    try {
        long status = dbFindRecord(&ent, pv);

        if(status) {
            return NULL; //cant find the PV.
        }

        CStr prefix;
        bool first = true;

        uint total_suggestions = 0;

        for (long status = dbFirstField(&ent, 1); !status; status= dbNextField(&ent, 1)) {
            //looping through fields. Check each name...
            const CStr name(ent.pflddes->name);

            if(!word.prefixOf(name))
                // input field is not a prefix of this field name... skip the below.
                continue;

            total_suggestions++;

            if(first) { // first match
                prefix = name;
                first = false;

            } else {
                prefix = prefix.commonPrefix(name, word.size());
            }
        }

        char** result;

        if (total_suggestions == 0) {
            result= getAllFields(&ent, pv);
            char * fullInput = (char *) malloc(pvlen + strlen(init_field) + 2u);
            snprintf(fullInput, pvlen + strlen(init_field) + 2u, "%s.", pv);
            strncat(fullInput, init_field, strlen(init_field));
            result[0] = fullInput;
            return result;
        } 

        char * fullPrefix = (char*) malloc(pvlen + prefix.size() + 2u);
        snprintf(fullPrefix, pvlen + prefix.size() + 2u, "%s.", pv);
        strncat(fullPrefix, prefix.p, prefix.size());

        result = (char**)malloc(sizeof(*result)*(2u + total_suggestions));
        result[0] = fullPrefix;
            
        int idx = 1;

        for (long status = dbFirstField(&ent, 1); !status; status= dbNextField(&ent, 1)) {
            const CStr name(ent.pflddes->name);

            if(!prefix.prefixOf(name))
                continue;

            //allocate some new memory to store the pv+.+name...
            char * fullname=(char* )malloc(pvlen + name.size() + 2u);
            
            snprintf(fullname, pvlen + name.size() + 2u, "%s.", pv);
            strncat(fullname, name.p, name.size());

            result[idx] = fullname;
            idx++;
        }
        result[total_suggestions+1] = NULL;

        dbFinishEntry(&ent);

        return result;
    } catch(std::exception& e){
        fprintf(stderr, "dbCompleteRecord error: %s\n", e.what());
        dbFinishEntry(&ent);
        return NULL;
    }

}
