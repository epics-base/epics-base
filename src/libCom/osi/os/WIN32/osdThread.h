
#ifndef osdThreadh
#define osdThreadh

#include <epicsAssert.h>

#ifndef VC_EXTRALEAN
#   define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN 
#   define WIN32_LEAN_AND_MEAN 
#endif
/* including less than this causes conflicts with winsock2.h :-( */
#include <winsock2.h>

typedef struct osdThreadPrivate 
{
    DWORD key;
} osdThreadPrivate;

epicsShareFunc INLINE threadVarId epicsShareAPI threadPrivateCreate ()
{
    osdThreadPrivate *p = (osdThreadPrivate *) malloc (sizeof (*p));
    if (p) {
        p->key = TlsAlloc ();
        if (p->key==0xFFFFFFFF) {
            free (p);
            p = 0;
        }
    }
    return (threadVarId) p;
}

epicsShareFunc INLINE void epicsShareAPI threadPrivateDelete (threadVarId id)
{
    osdThreadPrivate *p = (osdThreadPrivate *) id;
    BOOL stat = TlsFree (p->key);
    assert (stat);
}

epicsShareFunc INLINE void epicsShareAPI threadPrivateSet (threadVarId id, void *pVal)
{
    struct osdThreadPrivate *pPvt = (struct osdThreadPrivate *) id;
    BOOL stat = TlsSetValue (pPvt->key, (void *) pVal );
    assert (stat);
}

epicsShareFunc INLINE void * epicsShareAPI threadPrivateGet (threadVarId id)
{
    struct osdThreadPrivate *pPvt = (struct osdThreadPrivate *) id;
    return (void *) TlsGetValue (pPvt->key);
}

#endif /* osdThreadh */
