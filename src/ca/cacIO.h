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

#ifdef cacIOh_restore_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif


class cacChannel;

typedef unsigned long arrayElementCount;

// 1) this should not be passing caerr.h status to the exception callback
// 2) needless-to-say the data should be passed here using the new data access API
class epicsShareClass cacWriteNotify { // X aCC 655
public:
    virtual ~cacWriteNotify () = 0;
    virtual void completion () = 0;
// we should probably have a different vf for each type of exception ????
    virtual void exception ( int status, const char *pContext, 
        unsigned type, arrayElementCount count ) = 0;
};

// 1) this should not be passing caerr.h status to the exception callback
// 2) needless-to-say the data should be passed here using the new data access API
class epicsShareClass cacReadNotify { // X aCC 655
public:
    virtual ~cacReadNotify () = 0;
    virtual void completion ( unsigned type, 
        arrayElementCount count, const void *pData ) = 0;
// we should probably have a different vf for each type of exception ????
    virtual void exception ( int status, 
        const char *pContext, unsigned type, arrayElementCount count ) = 0;
};

// 1) this should not be passing caerr.h status to the exception callback
// 2) needless-to-say the data should be passed here using the new data access API
class epicsShareClass cacStateNotify { // X aCC 655
public:
    virtual ~cacStateNotify () = 0;
    virtual void current ( unsigned type, 
        arrayElementCount count, const void *pData ) = 0;
// we should probably have a different vf for each type of exception ????
    virtual void exception ( int status, 
        const char *pContext, unsigned type, arrayElementCount count ) = 0;
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

class epicsShareClass cacChannelNotify { // X aCC 655
public:
    virtual ~cacChannelNotify () = 0;
    virtual void connectNotify () = 0;
    virtual void disconnectNotify () = 0;
    virtual void serviceShutdownNotify () = 0;
    virtual void accessRightsNotify ( const caAccessRights & ) = 0;
    virtual void exception ( int status, const char *pContext ) = 0;
// we should probably have a different vf for each type of exception ????
    virtual void readException ( int status, const char *pContext,
        unsigned type, arrayElementCount count, void *pValue ) = 0;
// we should probably have a different vf for each type of exception ????
    virtual void writeException ( int status, const char *pContext,
        unsigned type, arrayElementCount count ) = 0;
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
    cacChannelNotify & notify () const;
    virtual void destroy () = 0;
    virtual const char * pName () const = 0; // not thread safe
    virtual void show ( unsigned level ) const = 0;
    virtual void initiateConnect () = 0;
    virtual ioStatus read ( unsigned type, arrayElementCount count, 
        cacReadNotify &, ioid * = 0 ) = 0;
    virtual void write ( unsigned type, arrayElementCount count, 
        const void *pValue ) = 0;
    virtual ioStatus write ( unsigned type, arrayElementCount count, 
        const void *pValue, cacWriteNotify &, ioid * = 0 ) = 0;
    virtual void subscribe ( unsigned type, arrayElementCount count, 
        unsigned mask, cacStateNotify &, ioid * = 0 ) = 0;
    virtual void ioCancel ( const ioid & ) = 0;
    virtual void ioShow ( const ioid &, unsigned level ) const = 0;
    virtual short nativeType () const = 0;
    virtual arrayElementCount nativeElementCount () const = 0;
    virtual caAccessRights accessRights () const; // defaults to unrestricted access
    virtual unsigned searchAttempts () const; // defaults to zero
    virtual double beaconPeriod () const; // defaults to negative DBL_MAX
    virtual bool ca_v42_ok () const; // defaults to true
    virtual bool connected () const; // defaults to true
    virtual void hostName (
        char * pBuf, unsigned bufLength ) const; // defaults to local host name
    virtual const char * pHostName () const; 

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
    void * operator new ( size_t );
    void operator delete ( void * );
};

class cacNotify { // X aCC 655
public:
    virtual ~cacNotify () = 0;
// we should probably have a different vf for each type of exception ????
    virtual void exception ( int status, const char * pContext, 
        const char * pFileName, unsigned lineNo ) = 0;
// perhaps this should be phased out in deference to the exception mechanism
    virtual int vPrintf ( const char *pformat, va_list args ) const = 0;
// backwards compatibility
    virtual void attachToClientCtx () = 0;
    virtual void blockForEventAndEnableCallbacks ( 
        class epicsEvent & event, const double & timeout ) = 0;
    virtual void messageArrivalNotify () = 0;
};

class cacService : public tsDLNode < cacService > { // X aCC 655
public:
    virtual cacChannel * createChannel ( 
        const char *pName, cacChannelNotify &, 
        cacChannel::priLev = cacChannel::priorityDefault ) = 0;
    virtual void show ( unsigned level ) const = 0;
};

class cacServiceList {
public:
	epicsShareFunc cacServiceList ();
    epicsShareFunc void registerService ( cacService &service );
    epicsShareFunc cacChannel * createChannel ( 
        const char *pName, cacChannelNotify &, 
        cacChannel::priLev = cacChannel::priorityDefault );
    epicsShareFunc void show ( unsigned level ) const;
private:
    tsDLList < cacService > services;
    mutable epicsMutex mutex;
	cacServiceList ( const cacServiceList & );
	cacServiceList & operator = ( const cacServiceList & );
};

template < class T > class epicsSingleton;
epicsShareExtern epicsSingleton < cacServiceList > globalServiceListCAC;

epicsShareFunc int epicsShareAPI ca_register_service ( cacService *pService );

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
