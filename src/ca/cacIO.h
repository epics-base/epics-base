
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
class cac;

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
    friend class cacNotifyIO;
};

class epicsShareClass cacNotifyIO {
public:
    cacNotifyIO ( cacNotify & );
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

class epicsShareClass cacChannel {
public:
    cacChannel ();
    virtual ~cacChannel () = 0;
    virtual void destroy () = 0;

    void attachIO ( class cacChannelIO &io );
    int read ( unsigned type, unsigned long count, void *pValue );
    int read ( unsigned type, unsigned long count, cacNotify & );
    int write ( unsigned type, unsigned long count, const void *pvalue );
    int write ( unsigned type, unsigned long count, const void *pvalue, cacNotify &notify );
    int subscribe ( unsigned type, unsigned long count, unsigned mask, cacNotify &notify );
    void hostName ( char *pBuf, unsigned bufLength ) const;
    short nativeType () const;
    unsigned long nativeElementCount () const;
    channel_state state () const;
    bool readAccess () const;
    bool writeAccess () const;
    const char *pName () const;
    unsigned searchAttempts () const;
    bool ca_v42_ok () const;
    bool connected () const;
    caar accessRights () const;
    unsigned readSequence () const;
    void incrementOutstandingIO ();
    void decrementOutstandingIO ();
    void decrementOutstandingIO ( unsigned seqNumber );

    const char * pHostName () const; // deprecated - please do not use

protected:
    class cacChannelIO *pChannelIO;

    void lockOutstandingIO () const;
    void unlockOutstandingIO () const;

private:
    virtual void ioAttachNotify ();
    virtual void ioReleaseNotify ();
    virtual void connectNotify ();
    virtual void disconnectNotify ();
    virtual void accessRightsNotify ( caar );
    virtual void exceptionNotify ( int status, const char *pContext );
    virtual void connectTimeoutNotify ();

    friend class cacChannelIO;
};

class epicsShareClass cacChannelIO {
public:
    cacChannelIO ( cacChannel &chan );
    virtual ~cacChannelIO () = 0;
    virtual void destroy () = 0;

    void connectNotify ();
    void disconnectNotify ();
    void connectTimeoutNotify ();
    void accessRightsNotify ( caar );
    void ioReleaseNotify ();

    virtual const char *pName () const = 0;

    virtual void lockOutstandingIO () const = 0;
    virtual void unlockOutstandingIO () const = 0;

    virtual void show ( unsigned level ) const = 0;

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
    virtual const char * pHostName () const; // deprecated - please do not use

    cacChannel &chan;

    friend class cacChannel;
};

class cacLocalChannelIO : 
    public cacChannelIO, public tsDLNode < cacLocalChannelIO > {
public:
    epicsShareFunc cacLocalChannelIO ( cac&, cacChannel &chan );
    epicsShareFunc virtual ~cacLocalChannelIO ();
private:
    cac &cacCtx;
};

struct cacServiceIO : public tsDLNode < cacServiceIO > {
public:
    epicsShareFunc virtual cacLocalChannelIO *createChannelIO ( const char *pName, cac &, cacChannel & ) = 0;
    epicsShareFunc virtual void show ( unsigned level ) const = 0;
private:
};

class cacServiceList : private osiMutex {
public:
    epicsShareFunc cacServiceList ();
    epicsShareFunc void registerService ( cacServiceIO &service );
    epicsShareFunc cacLocalChannelIO * createChannelIO ( const char *pName, cac &, cacChannel & );
    epicsShareFunc void show ( unsigned level ) const;
private:
    tsDLList < cacServiceIO > services;
};

epicsShareExtern cacServiceList cacGlobalServiceList;

epicsShareFunc int epicsShareAPI ca_register_service ( struct cacServiceIO *pService );


