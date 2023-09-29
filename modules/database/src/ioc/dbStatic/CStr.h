#include <algorithm>
#include <limits>
#include <string>
#include <set>

#include <string.h>

namespace {

// immutable C string slice.  (avoid allocating many temporary std::string)
class CStr {
public:
    const char* p;
    size_t l;

    CStr() :p(NULL), l(0u) {}
    CStr(const CStr& o) :p(o.p), l(o.l) {}
    explicit CStr(const char* p) :p(p), l(p ? strlen(p) : 0u) {}
    CStr(const char* p, size_t n) :p(p), l(n) {}

    CStr& operator=(const CStr& o) {
        p = o.p;
        l = o.l;
        return *this;
    }

    char* getP() {
        char* pval = (char*) malloc(l+1u);
        for (int i=0; i<l; i++) {
            pval[i] = p[i];
        }
        pval[l] = '\0';
        return pval;
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
