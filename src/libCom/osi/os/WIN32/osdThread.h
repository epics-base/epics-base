
#ifndef osdThreadh
#define osdThreadh

#include <assert.h>

#ifndef VC_EXTRALEAN
#   define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN 
#   define WIN32_LEAN_AND_MEAN 
#endif
/* including less than this causes conflicts with winsock2.h :-( */
#include <winsock2.h>

struct osdThreadPrivate 
{
#ifdef __cpluplus
    osdThreadPrivate ();
    ~osdThreadPrivate ();
#endif
    DWORD key;
};

#ifdef __cpluplus

template <class T>
inline T *osiThreadPrivate<T>::get ()
{
    return static_cast<T *> (TlsGetValue (this->key));
}

template <class T>
inline void osiThreadPrivate<T>::set (T *pIn)
{
    BOOL stat = TlsSetValue (this->key, static_cast<void *> (pIn) );
    assert (stat);
}

#else /* ifdef __cplusplus */

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

#endif /* ifdef/else __cplusplus */

#endif /* osdThreadh */
