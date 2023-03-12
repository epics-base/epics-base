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

namespace {

// immutable C string slice.  (avoid allocating many temporary std::string)
class CStr {
    const char* p;
    size_t l;
public:
    CStr() :p(NULL), l(0u) {}
    CStr(const CStr& o) :p(o.p), l(o.l) {}
    explicit CStr(const char* p) :p(p), l(p ? strlen(p) : 0u) {}
    CStr(const char* p, size_t n) :p(p), l(n) {}

    CStr& operator=(const CStr& o) {
        p = o.p;
        l = o.l;
        return *this;
    }

    bool operator==(const CStr& o) const {
        return l==o.l && (p==o.p || memcmp(p, o.p, l)==0);
    }
    bool operator!=(const CStr& o) const {
        return !(*this==o);
    }
    bool operator<(const CStr& o) const {
        size_t pl = std::min(l, o.l);
        int cmp = memcmp(p, o.p, pl);
        return cmp<0 || (cmp==0 && l < o.l);
    }

    bool empty() const { return !l; }
    size_t size() const { return l; }

    bool prefixOf(const CStr& full) const {
        return full.l >= l && memcmp(full.p, p, l)==0;
    }

    CStr commonPrefix(const CStr& o, size_t startFrom=0u) const {
        size_t n, N;
        for(n=startFrom, N=std::min(l, o.l); n<N; n++) {
            if(p[n]!=o.p[n])
                break;
        }
        return CStr(p, n);
    }

    void chop_at_first_of(const char* sep, size_t startFrom=0u) {
        size_t n;
        for(n=startFrom; n<l; n++) {
            char c = p[n];
            for(const char *s = sep; *s; s++) {
                if(c == *s) {
                    l = n+1u; // include trailing separator
                    return;
                }
            }
        }
    }

    char* dup() const {
        char* ret = (char*)malloc(l+1u);
        if(ret) {
            memcpy(ret, p, l);
            ret[l] = '\0';
        }
        return ret;
    }
};

} // namespace

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
