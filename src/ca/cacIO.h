
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include "tsDLList.h"
#include "osiMutex.h"

#include "shareLib.h"

class cacNotifyIO;

class epicsShareClass cacNotify {
public:
    cacNotify  ();
    virtual ~cacNotify () = 0;
    virtual void destroy () = 0;
    virtual void completionNotify ();
    virtual void completionNotify ( unsigned type, unsigned long count, const void *pData );
    virtual void exceptionNotify ( int status, const char *pContext );
    virtual void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
private:
    cacNotifyIO *pIO;
    virtual void lock ();
    virtual void unlock ();
    static osiMutex defaultMutex;
    friend class cacNotifyIO;
};

class epicsShareClass cacNotifyIO {
public:
    cacNotifyIO ( cacNotify &);
    virtual ~cacNotifyIO () = 0;
    virtual void destroy () = 0;
    void completionNotify ();
    void completionNotify ( unsigned type, unsigned long count, const void *pData );
    void exceptionNotify ( int status, const char *pContext );
    void exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count );
private:
    cacNotify &notify;
    friend class cacNotify;
};

class cacChannel;

class epicsShareClass cacChannelIO {
public:
    cacChannelIO ( cacChannel &chan );
    virtual ~cacChannelIO () = 0;
    virtual void destroy () = 0;

    void connectNotify ();
    void disconnectNotify ();
    void connectTimeoutNotify ();
    void accessRightsNotify ( caar );

    virtual const char *pName () const = 0;

    void lock ();
    void unlock ();

private:
    virtual int read ( unsigned type, unsigned long count, void *pValue) = 0;
    virtual int read ( unsigned type, unsigned long count, cacNotify &notify ) = 0;
    virtual int write ( unsigned type, unsigned long count, const void *pValue ) = 0;
    virtual int write ( unsigned type, unsigned long count, const void *pValue, cacNotify &notify ) = 0;
    virtual int subscribe ( unsigned type, unsigned long count, unsigned mask, cacNotify &notify ) = 0;
    virtual short nativeType () const = 0;
    virtual unsigned long nativeElementCount () const = 0;
    virtual void hostName (char *pBuf, unsigned bufLength) const; // defaults to local host name
    virtual channel_state state () const; // defaults to always connected
    virtual caar accessRights () const; // defaults to unrestricted access
    virtual unsigned searchAttempts () const; // defaults to zero
    virtual bool ca_v42_ok () const; // defaults to true
    virtual bool connected () const; // defaults to true
    virtual unsigned readSequence () const; // defaults to always zero
    virtual void incrementOutstandingIO ();
    virtual void decrementOutstandingIO ();

    cacChannel &chan;
    friend class cacChannel;
};

class epicsShareClass cacLocalChannelIO : 
    public cacChannelIO, public tsDLNode <cacLocalChannelIO> {
public:
    cacLocalChannelIO ( cacChannel &chan );
    virtual ~cacLocalChannelIO () = 0;
};

struct cacServiceIO : public tsDLNode <cacServiceIO> {
public:
    epicsShareFunc virtual cacLocalChannelIO *createChannelIO ( cacChannel &chan, const char *pName ) = 0;
private:
};

class cacServiceList : private osiMutex {
public:
    epicsShareFunc cacServiceList ();
    epicsShareFunc void registerService ( cacServiceIO &service );
    epicsShareFunc cacLocalChannelIO * createChannelIO (const char *pName, cacChannel &chan);
private:
    tsDLList <cacServiceIO> services;
};

epicsShareExtern cacServiceList cacGlobalServiceList;

epicsShareFunc int epicsShareAPI ca_register_service ( struct cacServiceIO *pService );


