
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

#ifndef cacIOh
#define cacIOh

//
// Open Issues
// -----------
//
// 1) A status code from the old client side interface is passed
// to the exception notify callback. Should we just pass a string?
// If so, then how do they detect the type of error and recover.
//
// 2) Some exception types are present here but there is no common
// exception base class in use.
//
// 3) Should I be passing the channel reference in cacChannelNotify?
//
// 4) Should the code for caAccessRights not be inline so that this
// interface is version independent.
//
//

#include <stdarg.h>

#include "tsDLList.h"
#include "epicsMutex.h"

#include "shareLib.h"

class cacChannel;

// this should not be passing caerr.h status to the exception callback
class epicsShareClass cacWriteNotify {
public:
    virtual ~cacWriteNotify () = 0;
    virtual void completion () = 0;
    virtual void exception ( int status, const char *pContext ) = 0;
};

// 1) this should not be passing caerr.h status to the exception callback
// 2) needless-to-say the data should be passed here using the new data access API
class epicsShareClass cacReadNotify {
public:
    virtual ~cacReadNotify () = 0;
    virtual void completion ( unsigned type, 
        unsigned long count, const void *pData ) = 0;
    virtual void exception ( int status, 
        const char *pContext, unsigned type, unsigned long count ) = 0;
};

// 1) this should not be passing caerr.h status to the exception callback
// 2) needless-to-say the data should be passed here using the new data access API
class epicsShareClass cacStateNotify {
public:
    virtual ~cacStateNotify () = 0;
    virtual void current ( unsigned type, 
        unsigned long count, const void *pData ) = 0;
    virtual void exception ( int status, 
        const char *pContext, unsigned type, unsigned long count ) = 0;
};

class caAccessRights {
public:
    caAccessRights ( 
        bool readPermit = false, 
        bool writePermit = false, 
        bool operatorConfirmationRequest = false);
    void setReadPermit ();
    void setWritePermit ();
    void setOperatorConfirmationRequest ();
    void clrReadPermit ();
    void clrWritePermit ();
    void clrOperatorConfirmationRequest ();
    bool readPermit () const;
    bool writePermit () const;
    bool operatorConfirmationRequest () const;
private:
    unsigned f_readPermit:1;
    unsigned f_writePermit:1;
    unsigned f_operatorConfirmationRequest:1;
};

class epicsShareClass cacChannelNotify {
public:
    virtual ~cacChannelNotify () = 0;
    virtual void connectNotify () = 0;
    virtual void disconnectNotify () = 0;
    virtual void accessRightsNotify ( const caAccessRights & ) = 0;
    virtual void exception ( int status, const char *pContext ) = 0;
    // not for public consumption -- can we get rid of this ????
    virtual bool includeFirstConnectInCountOfOutstandingIO () const;
};

//
// Notes
// 1) This interface assumes that when a channel is deleted then all
// attached IO is deleted. This is left over from the old interface,
// but perhaps is a bad practce that should be eliminated? If so,
// then the IO should not store or use a pointer to the channel.
//
class epicsShareClass cacChannel {
public:
    typedef unsigned ioid;
    enum ioStatus { iosSynch, iosAsynch };

    cacChannel ( cacChannelNotify & );
    cacChannelNotify & notify () const;
    virtual ~cacChannel () = 0;
    virtual const char *pName () const = 0;
    virtual void show ( unsigned level ) const = 0;
    virtual void initiateConnect () = 0;
    virtual void write ( unsigned type, unsigned long count, 
        const void *pValue ) = 0;
// we may need to include an optimization for read copy here if we want to enable
// reasonable performance of the old API. Adding it here means that the outstanding IO
// count must be visible :-(.
    virtual ioStatus read ( unsigned type, unsigned long count, 
        cacReadNotify &, ioid * = 0 ) = 0;
    virtual ioStatus write ( unsigned type, unsigned long count, 
        const void *pValue, cacWriteNotify &, ioid * = 0 ) = 0;
    virtual void subscribe ( unsigned type, unsigned long count, 
        unsigned mask, cacStateNotify &, ioid * = 0 ) = 0;
    virtual void ioCancel ( const ioid & ) = 0;
    virtual void ioShow ( const ioid &, unsigned level ) const = 0;
    virtual short nativeType () const = 0;
    virtual unsigned long nativeElementCount () const = 0;
    virtual caAccessRights accessRights () const; // defaults to unrestricted access
    virtual unsigned searchAttempts () const; // defaults to zero
    virtual double beaconPeriod () const; // defaults to negative DBL_MAX
    virtual bool ca_v42_ok () const; // defaults to true
    virtual bool connected () const; // defaults to true
    virtual bool previouslyConnected () const; // defaults to true
    virtual void hostName ( char *pBuf, unsigned bufLength ) const; // defaults to local host name

    virtual const char * pHostName () const; 
    virtual void notifyStateChangeFirstConnectInCountOfOutstandingIO ();

    // exceptions
    class badString {};
    class badType {};
    class outOfBounds {};
    class badEventSelection {};
    class noWriteAccess {};
    class noReadAccess {};
    class notConnected {};
    class unsupportedByService {};
    class noMemory {};

private:
    cacChannelNotify & callback;
};

class cacNotify {
public:
    virtual ~cacNotify () = 0;
// exception mechanism needs to be designed
    virtual void exception ( int status, const char *pContext,
        const char *pFileName, unsigned lineNo ) = 0;
    virtual void exception ( int status, const char *pContext,
        unsigned type, unsigned long count, 
        const char *pFileName, unsigned lineNo ) = 0;
// perhaps this should be phased out in deference to the exception mechanism
    virtual int vPrintf ( const char *pformat, va_list args ) = 0;
// this should probably be phased out (its not OS independent)
    virtual void fdWasCreated ( int fd ) = 0;
    virtual void fdWasDestroyed ( int fd ) = 0;
};

struct cacService : public tsDLNode < cacService > {
public:
    virtual cacChannel * createChannel ( 
        const char *pName, cacChannelNotify & ) = 0;
    virtual void show ( unsigned level ) const = 0;
};

class cacServiceList {
public:
    epicsShareFunc void registerService ( cacService &service );
    epicsShareFunc cacChannel * createChannel ( 
        const char *pName, cacChannelNotify & );
    epicsShareFunc void show ( unsigned level ) const;
private:
    tsDLList < cacService > services;
    mutable epicsMutex mutex;
};

epicsShareExtern cacServiceList cacGlobalServiceList;

epicsShareFunc int epicsShareAPI ca_register_service ( struct cacService *pService );

inline cacChannel::cacChannel ( cacChannelNotify &notify ) :
    callback ( notify )
{
}

inline cacChannelNotify & cacChannel::notify () const
{
    return this->callback;
}

inline caAccessRights::caAccessRights ( 
    bool readPermit, bool writePermit, bool operatorConfirmationRequest) :
    f_readPermit ( readPermit ), f_writePermit ( writePermit ), 
        f_operatorConfirmationRequest ( operatorConfirmationRequest ) {}

inline void caAccessRights::setReadPermit ()
{
    this->f_readPermit = true;
}

inline void caAccessRights::setWritePermit ()
{
    this->f_writePermit = true;
}

inline void caAccessRights::setOperatorConfirmationRequest ()
{
    this->f_operatorConfirmationRequest = true;
}

inline void caAccessRights::clrReadPermit ()
{
    this->f_readPermit = false;
}

inline void caAccessRights::clrWritePermit ()
{
    this->f_writePermit = false;
}

inline void caAccessRights::clrOperatorConfirmationRequest ()
{
    this->f_operatorConfirmationRequest = false;
}

inline bool caAccessRights::readPermit () const
{
    return this->f_readPermit;
}

inline bool caAccessRights::writePermit () const
{
    return this->f_writePermit;
}

inline bool caAccessRights::operatorConfirmationRequest () const
{
    return this->f_operatorConfirmationRequest;
}

#endif // ifndef cacIOh
