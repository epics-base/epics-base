
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
#include "epicsMutex.h"

#include "shareLib.h"

struct cacChannelIO;
struct cacNotifyIO;

// in the future we should probably not include the type and the count in this interface
class epicsShareClass cacNotify {
public:
    virtual ~cacNotify () = 0;
    virtual void release () = 0;
    virtual void completionNotify ( cacChannelIO & );
    virtual void completionNotify ( cacChannelIO &, unsigned type, 
        unsigned long count, const void *pData );
    virtual void exceptionNotify ( cacChannelIO &, 
        int status, const char *pContext );
    virtual void exceptionNotify ( cacChannelIO &, 
        int status, const char *pContext, unsigned type, unsigned long count );
};

// this name is probably poor
struct epicsShareClass cacNotifyIO {
public:
    cacNotifyIO ( cacNotify & );
    cacNotify & notify () const;
    virtual void cancel () = 0;
    virtual void show ( unsigned level ) const = 0;

    // the following commits us to deleting the IO when the channel is deleted :-(
    virtual cacChannelIO & channelIO () const = 0; 

protected:
    virtual ~cacNotifyIO () = 0;
private:
    cacNotify &callback;
};

class epicsShareClass cacChannelNotify {
public:
    virtual ~cacChannelNotify () = 0;
    virtual void release () = 0;
    virtual void connectNotify ( cacChannelIO & );
    virtual void disconnectNotify ( cacChannelIO & );
    virtual void accessRightsNotify ( cacChannelIO &, const caar & );
    virtual void exceptionNotify ( cacChannelIO &, int status, const char *pContext );

    // not for public consumption
    virtual bool includeFirstConnectInCountOfOutstandingIO () const;
    virtual class oldChannelNotify * pOldChannelNotify ();
};

//
// Notes
// 1) these routines should be changed to throw exceptions and not return
// ECA_XXXX style status in the future.
//
struct epicsShareClass cacChannelIO {
public:
    cacChannelIO ( cacChannelNotify & );
    cacChannelNotify & notify () const;
    virtual ~cacChannelIO () = 0;
    virtual const char *pName () const = 0;
    virtual void show ( unsigned level ) const = 0;
    virtual void initiateConnect () = 0;
    virtual int read ( unsigned type, 
        unsigned long count, cacNotify &notify ) = 0;
    virtual int write ( unsigned type, 
        unsigned long count, const void *pValue ) = 0;
    virtual int write ( unsigned type, 
        unsigned long count, const void *pValue, 
        cacNotify &notify ) = 0;
    virtual int subscribe ( unsigned type, 
        unsigned long count, unsigned mask, 
        cacNotify &notify, cacNotifyIO *& ) = 0;
    virtual short nativeType () const = 0;
    virtual unsigned long nativeElementCount () const = 0;
    virtual channel_state state () const; // defaults to always connected
    virtual caar accessRights () const; // defaults to unrestricted access
    virtual unsigned searchAttempts () const; // defaults to zero
    virtual double beaconPeriod () const; // defaults to negative DBL_MAX
    virtual bool ca_v42_ok () const; // defaults to true
    virtual bool connected () const; // defaults to true
    virtual void hostName (char *pBuf, unsigned bufLength) const; // defaults to local host name
    virtual const char * pHostName () const; // deprecated - please do not use
    virtual void notifyStateChangeFirstConnectInCountOfOutstandingIO (); // deprecated - please do not use
private:
    cacChannelNotify & callback;
};

struct cacServiceIO : public tsDLNode < cacServiceIO > {
public:
    virtual cacChannelIO *createChannelIO ( 
        const char *pName, cacChannelNotify & ) = 0;
    virtual void show ( unsigned level ) const = 0;
};

class cacServiceList {
public:
    epicsShareFunc void registerService ( cacServiceIO &service );
    epicsShareFunc cacChannelIO * createChannelIO ( 
        const char *pName, cacChannelNotify & );
    epicsShareFunc void show ( unsigned level ) const;
private:
    tsDLList < cacServiceIO > services;
    mutable epicsMutex mutex;
};

epicsShareExtern cacServiceList cacGlobalServiceList;

epicsShareFunc int epicsShareAPI ca_register_service ( struct cacServiceIO *pService );

inline cacNotifyIO::cacNotifyIO ( cacNotify &notify ) :
    callback ( notify )
{
}

inline cacNotify & cacNotifyIO::notify () const
{
    return this->callback;
}

inline cacChannelIO::cacChannelIO ( cacChannelNotify &notify ) :
    callback ( notify )
{
}

inline cacChannelNotify & cacChannelIO::notify () const
{
    return this->callback;
}

