
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

extern "C" void cacNoConnHandler ( struct connection_handler_args args );

class gnuOldAccessWarningEliminate {
};

struct oldChannel : public cacChannel {
public:
    oldChannel (caCh *pConnCallBack, void *pPrivate);
    void destroy ();
    void setPrivatePointer (void *);
    void * privatePointer () const;
    int changeConnCallBack (caCh *pfunc);
    int replaceAccessRightsEvent (caArh *pfunc);
    void ioAttachNotify ();
    void ioReleaseNotify ();

    void * operator new (size_t size);
    void operator delete (void *pCadaver, size_t size);

private:
    caCh *pConnCallBack;
    void *pPrivate;
    caArh *pAccessRightsFunc;

    ~oldChannel (); // must allocate from pool
    void connectTimeoutNotify ();
    void connectNotify ();
    void disconnectNotify ();
    void accessRightsNotify ( caar );
    static tsFreeList < struct oldChannel, 1024 > freeList;

    friend int epicsShareAPI ca_array_get (chtype type, unsigned long count, chid pChan, void *pValue);
    friend void cacNoConnHandler ( struct connection_handler_args args );
};

class getCallback : public cacNotify {
public:
    getCallback (oldChannel &chan, caEventCallBackFunc *pFunc, void *pPrivate);
    void destroy ();

    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );

private:
    oldChannel &chan;
    caEventCallBackFunc *pFunc;
    void *pPrivate;
    ~getCallback (); // allocate only out of pool
    void completionNotify ();
    void completionNotify (unsigned type, unsigned long count, const void *pData);
    void exceptionNotify (int status, const char *pContext);
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    static tsFreeList < class getCallback, 1024 > freeList;
    friend class gnuWarningEliminate;
};

class putCallback : public cacNotify {
public:
    putCallback (oldChannel &chan, caEventCallBackFunc *pFunc, void *pPrivate );
    void destroy ();

    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );

private:
    oldChannel &chan;
    caEventCallBackFunc *pFunc;
    void *pPrivate;
    ~putCallback (); // allocate only out of pool
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
    static tsFreeList < class putCallback, 1024 > freeList;
    friend class gnuWarningEliminate;
};

struct oldSubscription : public cacNotify {
public:
    oldSubscription  ( oldChannel &chan, caEventCallBackFunc *pFunc, void *pPrivate );
    void destroy ();
    oldChannel &channel ();

    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );

private:
    oldChannel &chan;

    caEventCallBackFunc *pFunc;
    void *pPrivate;

    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );

    ~oldSubscription (); // must allocate from pool
    static tsFreeList < struct oldSubscription, 1024 > freeList;
    friend class gnuWarningEliminate;
};

