
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */


class oldChannelNotify : public cacChannelNotify {
public:
    oldChannelNotify ( caCh *pConnCallBackIn, void *pPrivateIn );
    void release ();
    void setPrivatePointer ( void * );
    void * privatePointer () const;
    int changeConnCallBack ( cacChannelIO &, caCh *pfunc );
    int replaceAccessRightsEvent ( cacChannelIO &chan, caArh *pfunc );

    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~oldChannelNotify (); // must allocate from pool
private:
    caCh *pConnCallBack;
    void *pPrivate;
    caArh *pAccessRightsFunc;
    void connectNotify ( cacChannelIO &chan );
    void disconnectNotify ( cacChannelIO &chan );
    void accessRightsNotify ( cacChannelIO &chan, const caar & );
    bool includeFirstConnectInCountOfOutstandingIO () const;
    class oldChannelNotify * pOldChannelNotify ();
    static tsFreeList < class oldChannelNotify, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class getCopy : public cacNotify {
public:
    getCopy ( cac &cacCtx, unsigned type, 
        unsigned long count, void *pValue );
    void release ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    void show ( unsigned level ) const;
protected:
    ~getCopy (); // allocate only out of pool
private:
    unsigned long count;
    cac &cacCtx;
    void *pValue;
    unsigned readSeq;
    unsigned type;
    void completionNotify ( cacChannelIO & );
    void completionNotify ( cacChannelIO &, 
        unsigned type, unsigned long count, const void *pData);
    void exceptionNotify ( cacChannelIO &, 
        int status, const char *pContext);
    void exceptionNotify ( cacChannelIO &, 
        int status, const char *pContext, unsigned type, unsigned long count );
    static tsFreeList < class getCopy, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class getCallback : public cacNotify {
public:
    getCallback ( caEventCallBackFunc *pFunc, void *pPrivate );
    void release ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~getCallback (); // allocate only out of pool
private:
    caEventCallBackFunc *pFunc;
    void *pPrivate;
    void completionNotify ( cacChannelIO & );
    void completionNotify ( cacChannelIO &, 
        unsigned type, unsigned long count, const void *pData);
    void exceptionNotify ( cacChannelIO &, 
        int status, const char *pContext);
    void exceptionNotify ( cacChannelIO &, 
        int status, const char *pContext, unsigned type, unsigned long count );
    static tsFreeList < class getCallback, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class putCallback : public cacNotify {
public:
    putCallback ( caEventCallBackFunc *pFunc, void *pPrivate );
    void release ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~putCallback (); // allocate only out of pool
private:
    caEventCallBackFunc *pFunc;
    void *pPrivate;
    void completionNotify ( cacChannelIO & );
    void completionNotify ( cacChannelIO &, 
        unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( cacChannelIO &, 
        int status, const char *pContext );
    void exceptionNotify ( cacChannelIO &, 
        int status, const char *pContext, unsigned type, unsigned long count );
    static tsFreeList < class putCallback, 1024 > freeList;
    static epicsMutex freeListMutex;
};

struct oldSubscription : public cacNotify {
public:
    oldSubscription ( caEventCallBackFunc *pFunc, void *pPrivate );
    void release ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~oldSubscription (); // must allocate from pool
private:
    caEventCallBackFunc *pFunc;
    void *pPrivate;
    void completionNotify ( cacChannelIO & );
    void completionNotify ( cacChannelIO &, 
        unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( cacChannelIO &, 
        int status, const char *pContext );
    void exceptionNotify ( cacChannelIO &, 
        int status, const char *pContext, unsigned type, unsigned long count );
    static tsFreeList < struct oldSubscription, 1024 > freeList;
    static epicsMutex freeListMutex;
};

inline getCallback::getCallback ( caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
    pFunc ( pFuncIn ), pPrivate ( pPrivateIn )
{
}

inline void * getCopy::operator new ( size_t size )
{
    epicsAutoMutex locker ( getCopy::freeListMutex );
    return getCopy::freeList.allocate ( size );
}

inline void getCopy::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( getCopy::freeListMutex );
    getCopy::freeList.release ( pCadaver, size );
}

inline void * getCallback::operator new ( size_t size )
{
    epicsAutoMutex locker ( getCallback::freeListMutex );
    return getCallback::freeList.allocate ( size );
}

inline void getCallback::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( getCallback::freeListMutex );
    getCallback::freeList.release ( pCadaver, size );
}

inline putCallback::putCallback ( caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
    pFunc ( pFuncIn ), pPrivate ( pPrivateIn )
{
}

inline void * putCallback::operator new ( size_t size )
{
    epicsAutoMutex locker ( putCallback::freeListMutex );
    return putCallback::freeList.allocate ( size );
}

inline void putCallback::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( putCallback::freeListMutex );
    putCallback::freeList.release ( pCadaver, size );
}

inline oldSubscription::oldSubscription  ( caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
    pFunc ( pFuncIn ), pPrivate ( pPrivateIn )
{
}

inline void * oldSubscription::operator new ( size_t size )
{
    epicsAutoMutex locker ( oldSubscription::freeListMutex );
    return oldSubscription::freeList.allocate ( size );
}

inline void oldSubscription::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( oldSubscription::freeListMutex );
    oldSubscription::freeList.release ( pCadaver, size );
}
