/*************************************************************************\
* Copyright (c) 2022 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <algorithm>
#include <limits>
#include <string>
#include <set>

#include <string.h>

#include <epicsStdio.h>
#include <dbAccess.h>
#include "dbStaticPvt.h"
#include <CStr.h>



char** dbCompleteRecord(const char *cword)
{
    const CStr word(cword);

    DBENTRY ent;

    dbInitEntry(pdbbase, &ent);
    try{
        // iterating all record twice...

        CStr prefix;
        bool first = true;

        // find longest prefix match
        for(long status = dbFirstRecordType(&ent); !status; status = dbNextRecordType(&ent)) {
            for(status = dbFirstRecord(&ent); !status; status = dbNextRecord(&ent)) {
                const CStr name(ent.precnode->recordname);

                if(!word.prefixOf(name))
                    continue;

                if(first) { // first match
                    prefix = name;
                    first = false;

                } else {
                    prefix = prefix.commonPrefix(name, word.size());
                }
            }
        }

        // with prefix size known, iterate again to find suggestions
        typedef std::set<CStr> suggestions_t;
        suggestions_t suggestions;

        for(long status = dbFirstRecordType(&ent); !status; status = dbNextRecordType(&ent)) {
            for(status = dbFirstRecord(&ent); !status; status = dbNextRecord(&ent)) {
                CStr name(ent.precnode->recordname);

                if(!prefix.prefixOf(name))
                    continue;

                name.chop_at_first_of(":<>{}-", prefix.size());
                suggestions.insert(name);
            }
        }

        dbFinishEntry(&ent);

        char** ret = NULL;

        if(!prefix.empty() || !suggestions.empty()) {
            ret = (char**)malloc(sizeof(*ret)*(2u + suggestions.size()));
            if(ret) {
                ret[0] = prefix.dup();
                size_t n=1u;
                for(suggestions_t::iterator it(suggestions.begin()), end(suggestions.end());
                    it!=end; ++it)
                {
                    ret[n++] = it->dup();
                }
                ret[n] = NULL;
            }
        }

        return ret;

    } catch(std::exception& e){
        fprintf(stderr, "dbCompleteRecord error: %s\n", e.what());
        dbFinishEntry(&ent);
        return NULL;
    }

}
