
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

//
// Notes
// 1) these routines should be changed to throw exceptions and not return
// ECA_XXXX style status in the future.
//

#include "tsDLList.h"
#include "epicsMutex.h"

#include "shareLib.h"

struct cacChannelIO;
struct cacNotifyIO;

class cacNotify {
public:
    epicsShareFunc virtual void release () = 0;
    epicsShareFunc virtual void completionNotify ( cacChannelIO & );
    epicsShareFunc virtual void completionNotify ( cacChannelIO &, unsigned type, 
        unsigned long count, const void *pData );
    epicsShareFunc virtual void exceptionNotify ( cacChannelIO &, 
        int status, const char *pContext );
    epicsShareFunc virtual void exceptionNotify ( cacChannelIO &, 
        int status, const char *pContext, unsigned type, unsigned long count );
protected:
    epicsShareFunc virtual ~cacNotify () = 0;
};

struct cacNotifyIO {
public:
    epicsShareFunc cacNotifyIO ( cacNotify & );
    epicsShareFunc cacNotify & notify () const;
    epicsShareFunc virtual void destroy () = 0; // also uninstalls
    epicsShareFunc virtual cacChannelIO & channelIO () const = 0;
protected:
    epicsShareFunc virtual ~cacNotifyIO () = 0;
private:
    cacNotify &callback;
};

class cacChannelNotify {
public:
    epicsShareFunc virtual void release () = 0;
    epicsShareFunc virtual void connectNotify ( cacChannelIO & );
    epicsShareFunc virtual void disconnectNotify ( cacChannelIO & );
    epicsShareFunc virtual void accessRightsNotify ( cacChannelIO &, const caar & );
    epicsShareFunc virtual void exceptionNotify ( cacChannelIO &, int status, const char *pContext );

    // not for public consumption
    epicsShareFunc virtual bool includeFirstConnectInCountOfOutstandingIO () const;
    epicsShareFunc virtual class oldChannelNotify * pOldChannelNotify ();
protected:
    epicsShareFunc virtual ~cacChannelNotify () = 0;
};

struct cacChannelIO {
public:
    epicsShareFunc cacChannelIO ( cacChannelNotify &chan );
    epicsShareFunc cacChannelNotify & notify () const;

    epicsShareFunc virtual void destroy () = 0;
    epicsShareFunc virtual const char *pName () const = 0;
    epicsShareFunc virtual void show ( unsigned level ) const = 0;
    epicsShareFunc virtual void initiateConnect () = 0;
    epicsShareFunc virtual int read ( unsigned type, 
        unsigned long count, void *pValue) = 0;
    epicsShareFunc virtual int read ( unsigned type, 
        unsigned long count, cacNotify &notify ) = 0;
    epicsShareFunc virtual int write ( unsigned type, 
        unsigned long count, const void *pValue ) = 0;
    epicsShareFunc virtual int write ( unsigned type, 
        unsigned long count, const void *pValue, 
        cacNotify &notify ) = 0;
    epicsShareFunc virtual int subscribe ( unsigned type, 
        unsigned long count, unsigned mask, 
        cacNotify &notify, cacNotifyIO *& ) = 0;
    epicsShareFunc virtual short nativeType () const = 0;
    epicsShareFunc virtual unsigned long nativeElementCount () const = 0;
    epicsShareFunc virtual channel_state state () const; // defaults to always connected
    epicsShareFunc virtual caar accessRights () const; // defaults to unrestricted access
    epicsShareFunc virtual unsigned searchAttempts () const; // defaults to zero
    epicsShareFunc virtual double beaconPeriod () const; // defaults to negative DBL_MAX
    epicsShareFunc virtual bool ca_v42_ok () const; // defaults to true
    epicsShareFunc virtual bool connected () const; // defaults to true
    epicsShareFunc virtual void hostName (char *pBuf, unsigned bufLength) const; // defaults to local host name
    epicsShareFunc virtual const char * pHostName () const; // deprecated - please do not use
    epicsShareFunc virtual void notifyStateChangeFirstConnectInCountOfOutstandingIO ();
protected:
    epicsShareFunc virtual ~cacChannelIO () = 0;
private:
    cacChannelNotify &chan;
};

class cac;

struct cacServiceIO : public tsDLNode < cacServiceIO > {
public:
    epicsShareFunc virtual cacChannelIO *createChannelIO ( 
        const char *pName, cac &, cacChannelNotify & ) = 0;
    epicsShareFunc virtual void show ( unsigned level ) const = 0;
private:
};

class cacServiceList : private epicsMutex {
public:
    epicsShareFunc cacServiceList ();
    epicsShareFunc void registerService ( cacServiceIO &service );
    epicsShareFunc cacChannelIO * createChannelIO ( 
        const char *pName, cac &, cacChannelNotify & );
    epicsShareFunc void show ( unsigned level ) const;
private:
    tsDLList < cacServiceIO > services;
};

epicsShareExtern cacServiceList cacGlobalServiceList;

epicsShareFunc int epicsShareAPI ca_register_service ( struct cacServiceIO *pService );

inline cacNotifyIO::cacNotifyIO ( cacNotify &notifyIn ) : callback ( notifyIn )
{
}

inline cacNotify & cacNotifyIO::notify () const
{
    return this->callback;
}

inline cacChannelIO::cacChannelIO ( cacChannelNotify &chanIn ) : chan ( chanIn ) 
{
}

inline cacChannelNotify & cacChannelIO::notify () const
{
    return this->chan;
}

