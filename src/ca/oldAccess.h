
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
};

inline getCallback::getCallback ( caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
    pFunc ( pFuncIn ), pPrivate ( pPrivateIn )
{
}

inline void * getCallback::operator new ( size_t size )
{
    return getCallback::freeList.allocate ( size );
}

inline void getCallback::operator delete ( void *pCadaver, size_t size )
{
    getCallback::freeList.release ( pCadaver, size );
}

inline putCallback::putCallback ( caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
    pFunc ( pFuncIn ), pPrivate ( pPrivateIn )
{
}

inline void * putCallback::operator new ( size_t size )
{
    return putCallback::freeList.allocate ( size );
}

inline void putCallback::operator delete ( void *pCadaver, size_t size )
{
    putCallback::freeList.release ( pCadaver, size );
}

inline oldSubscription::oldSubscription  ( caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
    pFunc ( pFuncIn ), pPrivate ( pPrivateIn )
{
}

inline void * oldSubscription::operator new ( size_t size )
{
    return oldSubscription::freeList.allocate ( size );
}

inline void oldSubscription::operator delete ( void *pCadaver, size_t size )
{
    oldSubscription::freeList.release ( pCadaver, size );
}
