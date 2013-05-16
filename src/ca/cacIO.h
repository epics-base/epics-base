/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*  
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
// Perhaps we should  call a different vf for each type of exception.
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

#include <new>
#include <stdarg.h>

#ifdef epicsExportSharedSymbols
#   define cacIOh_restore_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "tsDLList.h"
#include "epicsMutex.h"
#include "epicsGuard.h"
#include "epicsThread.h"

#ifdef cacIOh_restore_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif


class cacChannel;

typedef unsigned long arrayElementCount;

// 1) this should not be passing caerr.h status to the exception callback
// 2) needless-to-say the data should be passed here using the new data access API
class epicsShareClass cacWriteNotify {
public:
    virtual ~cacWriteNotify () = 0;
    virtual void completion ( epicsGuard < epicsMutex > & ) = 0;
// we should probably have a different vf for each type of exception ????
    virtual void exception ( 
        epicsGuard < epicsMutex > &,
        int status, const char * pContext, 
        unsigned type, arrayElementCount count ) = 0;
};

// 1) this should not be passing caerr.h status to the exception callback
// 2) needless-to-say the data should be passed here using the new data access API
class epicsShareClass cacReadNotify {
public:
    virtual ~cacReadNotify () = 0;
    virtual void completion ( 
        epicsGuard < epicsMutex > &, unsigned type, 
        arrayElementCount count, const void * pData ) = 0;
// we should probably have a different vf for each type of exception ????
    virtual void exception ( 
        epicsGuard < epicsMutex > &, int status, 
        const char * pContext, unsigned type, 
        arrayElementCount count ) = 0;
};

// 1) this should not be passing caerr.h status to the exception callback
// 2) needless-to-say the data should be passed here using the new data access API
class epicsShareClass cacStateNotify {
public:
    virtual ~cacStateNotify () = 0;
    virtual void current ( 
        epicsGuard < epicsMutex > &, unsigned type, 
        arrayElementCount count, const void * pData ) = 0;
// we should probably have a different vf for each type of exception ????
    virtual void exception ( 
        epicsGuard < epicsMutex > &, int status, 
        const char *pContext, unsigned type, 
        arrayElementCount count ) = 0;
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
    bool f_readPermit:1;
    bool f_writePermit:1;
    bool f_operatorConfirmationRequest:1;
};

class epicsShareClass cacChannelNotify {
public:
    virtual ~cacChannelNotify () = 0;
    virtual void connectNotify ( epicsGuard < epicsMutex > & ) = 0;
    virtual void disconnectNotify ( epicsGuard < epicsMutex > & ) = 0;
    virtual void serviceShutdownNotify (
        epicsGuard < epicsMutex > & mutualExclusionGuard ) = 0;
    virtual void accessRightsNotify ( 
        epicsGuard < epicsMutex > &, const caAccessRights & ) = 0;
    virtual void exception ( 
        epicsGuard < epicsMutex > &, int status, const char *pContext ) = 0;
// we should probably have a different vf for each type of exception ????
    virtual void readException ( 
        epicsGuard < epicsMutex > &, int status, const char *pContext,
        unsigned type, arrayElementCount count, void *pValue ) = 0;
// we should probably have a different vf for each type of exception ????
    virtual void writeException ( 
        epicsGuard < epicsMutex > &, int status, const char * pContext,
        unsigned type, arrayElementCount count ) = 0;
};

class CallbackGuard :
    public epicsGuard < epicsMutex > {
public:
    CallbackGuard ( epicsMutex & mutex ) :
        epicsGuard < epicsMutex > ( mutex ) {}
private:
    CallbackGuard ( const CallbackGuard & );
    CallbackGuard & operator = ( const CallbackGuard & );
};

//
// Notes
// 1) This interface assumes that when a channel is deleted then all
// attached IO is deleted. This is left over from the old interface,
// but perhaps is a bad practice that should be eliminated? If so,
// then the IO should not store or use a pointer to the channel.
//
class epicsShareClass cacChannel {
public:
    typedef unsigned priLev;
    static const priLev priorityMax;
    static const priLev priorityMin;
    static const priLev priorityDefault;
    static const priLev priorityLinksDB;
    static const priLev priorityArchive;
    static const priLev priorityOPI;

    typedef unsigned ioid;
    enum ioStatus { iosSynch, iosAsynch };

    cacChannel ( cacChannelNotify & );
    virtual void destroy (
        CallbackGuard & callbackGuard,
        epicsGuard < epicsMutex > & mutualExclusionGuard ) = 0;
    cacChannelNotify & notify () const; // required ?????
    virtual unsigned getName (
        epicsGuard < epicsMutex > &, 
        char * pBuf, unsigned bufLen ) const throw () = 0;
    // !! deprecated, avoid use  !!
    virtual const char * pName (
        epicsGuard < epicsMutex > & guard ) const throw () = 0;
    virtual void show ( 
        epicsGuard < epicsMutex > &,
        unsigned level ) const = 0;
    virtual void initiateConnect (
        epicsGuard < epicsMutex > & ) = 0;
    virtual unsigned requestMessageBytesPending ( 
        epicsGuard < epicsMutex > & mutualExclusionGuard ) = 0;
    virtual void flush ( 
        epicsGuard < epicsMutex > & mutualExclusionGuard ) = 0;
    virtual ioStatus read ( 
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count, 
        cacReadNotify &, ioid * = 0 ) = 0;
    virtual void write ( 
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count, 
        const void *pValue ) = 0;
    virtual ioStatus write ( 
        epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count, 
        const void *pValue, cacWriteNotify &, ioid * = 0 ) = 0;
    virtual void subscribe ( 
        epicsGuard < epicsMutex > &, unsigned type, 
        arrayElementCount count, unsigned mask, cacStateNotify &, 
        ioid * = 0 ) = 0;
    // The primary mutex must be released when calling the user's
    // callback, and therefore a finite interval exists when we are
    // moving forward with the intent to call the users callback
    // but the users IO could be deleted during this interval.
    // To prevent the user's callback from being called after
    // destroying his IO we must past a guard for the callback
    // mutex here.
    virtual void ioCancel ( 
        CallbackGuard & callbackGuard,
        epicsGuard < epicsMutex > & mutualExclusionGuard,
        const ioid & ) = 0;
    virtual void ioShow ( 
        epicsGuard < epicsMutex > &,
        const ioid &, unsigned level ) const = 0;
    virtual short nativeType (
        epicsGuard < epicsMutex > & ) const = 0;
    virtual arrayElementCount nativeElementCount (
        epicsGuard < epicsMutex > & ) const = 0;
    virtual caAccessRights accessRights (
        epicsGuard < epicsMutex > & ) const;
    virtual unsigned searchAttempts (
        epicsGuard < epicsMutex > & ) const;
    virtual double beaconPeriod (
        epicsGuard < epicsMutex > & ) const; // negative DBL_MAX if UKN
    virtual double receiveWatchdogDelay (
        epicsGuard < epicsMutex > & ) const; // negative DBL_MAX if UKN
    virtual bool ca_v42_ok (
        epicsGuard < epicsMutex > & ) const; 
    virtual bool connected (
        epicsGuard < epicsMutex > & ) const; 
    virtual unsigned getHostName (
        epicsGuard < epicsMutex > &,
        char * pBuf, unsigned bufLength ) const throw ();
    // !! deprecated, avoid use  !!
    virtual const char * pHostName (
        epicsGuard < epicsMutex > & guard ) const throw ();

    // exceptions
    class badString {};
    class badType {};
    class badPriority {};
    class outOfBounds {};
    class badEventSelection {};
    class noWriteAccess {};
    class noReadAccess {};
    class notConnected {};
    class unsupportedByService {};
    class msgBodyCacheTooSmall {}; // hopefully this one goes away in the future
    class requestTimedOut {};

protected:
    virtual ~cacChannel () = 0;

private:
    cacChannelNotify & callback;
	cacChannel ( const cacChannel & );
	cacChannel & operator = ( const cacChannel & );
};

class epicsShareClass cacContext {
public:
    virtual ~cacContext ();
    virtual cacChannel & createChannel ( 
        epicsGuard < epicsMutex > &,
        const char * pChannelName, cacChannelNotify &, 
        cacChannel::priLev = cacChannel::priorityDefault ) = 0;
    virtual void flush ( 
        epicsGuard < epicsMutex > & ) = 0;
    virtual unsigned circuitCount (
        epicsGuard < epicsMutex > & ) const = 0;
    virtual void selfTest (
        epicsGuard < epicsMutex > & ) const = 0;
    virtual unsigned beaconAnomaliesSinceProgramStart (
        epicsGuard < epicsMutex > & ) const = 0;
    virtual void show ( 
        epicsGuard < epicsMutex > &, unsigned level ) const = 0;
};

class epicsShareClass cacContextNotify {
public:
    virtual ~cacContextNotify () = 0;
    virtual cacContext & createNetworkContext ( 
        epicsMutex & mutualExclusion, epicsMutex & callbackControl ) = 0;
// we should probably have a different vf for each type of exception ????
    virtual void exception ( 
        epicsGuard < epicsMutex > &, int status, const char * pContext, 
        const char * pFileName, unsigned lineNo ) = 0;
// perhaps this should be phased out in deference to the exception mechanism
    virtual int varArgsPrintFormated ( const char * pformat, va_list args ) const = 0;
// backwards compatibility (from here down)
    virtual void attachToClientCtx () = 0;
    virtual void callbackProcessingInitiateNotify () = 0;
    virtual void callbackProcessingCompleteNotify () = 0;
};

// **** Lock Hierarchy ****
// callbackControl must be taken before mutualExclusion if both are held at
// the same time
class epicsShareClass cacService {
public:
    virtual ~cacService () = 0;
    virtual cacContext & contextCreate ( 
        epicsMutex & mutualExclusion, 
        epicsMutex & callbackControl, 
        cacContextNotify & ) = 0;
};

epicsShareFunc void epicsShareAPI caInstallDefaultService ( cacService & service );

epicsShareExtern epicsThreadPrivateId caClientCallbackThreadId;

inline cacChannel::cacChannel ( cacChannelNotify & notify ) :
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
