/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
//  Example EPICS CA server
//
//
//  caServer
//  |
//  exServer
//
//  casPV
//  |
//  exPV-----------
//  |             |
//  exScalarPV    exVectorPV
//  |
//  exAsyncPV
//
//  casChannel
//  |
//  exChannel
//


//
// ANSI C
//
#include <string.h>
#include <stdio.h>

//
// EPICS
//
#include "gddAppFuncTable.h"
#include "epicsTimer.h"
#include "casdef.h"
#include "epicsAssert.h"
#include "resourceLib.h"
#include "tsMinMax.h"

#ifndef NELEMENTS
#   define NELEMENTS(A) (sizeof(A)/sizeof(A[0]))
#endif

#define maxSimultAsyncIO 1000u

//
// info about all pv in this server
//
enum excasIoType { excasIoSync, excasIoAsync };

class exPV;
class exServer;

//
// pvInfo
//
class pvInfo {
public:
    pvInfo ( double scanPeriodIn, const char *pNameIn, 
            aitFloat32 hoprIn, aitFloat32 loprIn,
            aitEnum typeIn, excasIoType ioTypeIn, 
            unsigned countIn );
    pvInfo ( const pvInfo & copyIn );
    ~pvInfo ();
    double getScanPeriod () const;
    const char *getName () const;
    double getHopr () const;
    double getLopr () const;
    aitEnum getType () const;
    excasIoType getIOType () const;
    unsigned getElementCount () const;
    void unlinkPV ();
    exPV *createPV ( exServer & exCAS, 
        bool preCreateFlag, bool scanOn );
    void deletePV ();

private:
    const double scanPeriod;
    const char * pName;
    const double hopr;
    const double lopr;
    aitEnum type;
    const excasIoType ioType;
    const unsigned elementCount;
    exPV * pPV;
};

//
// pvEntry 
//
// o entry in the string hash table for the pvInfo
// o Since there may be aliases then we may end up
// with several of this class all referencing 
// the same pv info class (justification
// for this breaking out into a seperate class
// from pvInfo)
//
class pvEntry // X aCC 655
    : public stringId, public tsSLNode < pvEntry > {
public:
    pvEntry ( pvInfo  &infoIn, exServer & casIn, 
            const char * pAliasName );
    ~pvEntry();
    pvInfo & getInfo() const { return this->info; }
    void destroy ();

private:
    pvInfo & info;
    exServer & cas;
};


//
// exPV
//
class exPV : public casPV, public epicsTimerNotify, 
    public tsSLNode < exPV > {
public:
    exPV ( pvInfo & setup, bool preCreateFlag, bool scanOn );
    virtual ~exPV();

    void show ( unsigned level ) const;

    //
    // Called by the server libary each time that it wishes to
    // subscribe for PV the server tool via postEvent() below.
    //
    caStatus interestRegister ();

    //
    // called by the server library each time that it wishes to
    // remove its subscription for PV value change events
    // from the server tool via caServerPostEvents()
    //
    void interestDelete ();

    aitEnum bestExternalType () const;

    //
    // chCreate() is called each time that a PV is attached to
    // by a client. The server tool must create a casChannel object
    // (or a derived class) each time that this routine is called
    //
    // If the operation must complete asynchronously then return
    // the status code S_casApp_asyncCompletion and then
    // create the casChannel object at some time in the future
    //
    //casChannel *createChannel ();

    //
    // This gets called when the pv gets a new value
    //
    caStatus update ( smartConstGDDPointer pValue );

    //
    // Gets called when we add noise to the current value
    //
    virtual void scan () = 0;
    
    //
    // If no one is watching scan the PV with 10.0
    // times the specified period
    //
    double getScanPeriod ();

    caStatus read ( const casCtx &, gdd & protoIn );

    caStatus readNoCtx ( smartGDDPointer pProtoIn );

    caStatus write ( const casCtx &, const gdd & value );

    void destroy ();

    const pvInfo & getPVInfo ();

    const char * getName() const;

    static void initFT();

    casChannel * createChannel ( const casCtx &ctx,
        const char * const pUserName, 
        const char * const pHostName );

protected:
    smartConstGDDPointer pValue;
    epicsTimer & timer;
    pvInfo & info; 
    bool interest;
    bool preCreate;
    bool scanOn;
    static epicsTime currentTime;

    virtual caStatus updateValue ( smartConstGDDPointer pValue ) = 0;

private:

    //
    // scan timer expire
    //
    expireStatus expire ( const epicsTime & currentTime );

    //
    // Std PV Attribute fetch support
    //
    gddAppFuncTableStatus getPrecision(gdd &value);
    gddAppFuncTableStatus getHighLimit(gdd &value);
    gddAppFuncTableStatus getLowLimit(gdd &value);
    gddAppFuncTableStatus getUnits(gdd &value);
    gddAppFuncTableStatus getValue(gdd &value);
    gddAppFuncTableStatus getEnums(gdd &value);

    //
    // static
    //
    static gddAppFuncTable<exPV> ft;
    static char hasBeenInitialized;
};

//
// exScalarPV
//
class exScalarPV : public exPV {
public:
    exScalarPV ( pvInfo &setup, bool preCreateFlag, bool scanOnIn ) :
            exPV ( setup, preCreateFlag, scanOnIn) {}
    void scan();
private:
    caStatus updateValue (smartConstGDDPointer pValue);
};

//
// exVectorPV
//
class exVectorPV : public exPV {
public:
    exVectorPV ( pvInfo &setup, bool preCreateFlag, bool scanOnIn ) :
            exPV ( setup, preCreateFlag, scanOnIn) {}
    void scan();

    unsigned maxDimension() const;
    aitIndex maxBound (unsigned dimension) const;

private:
    caStatus updateValue (smartConstGDDPointer pValue);
};

//
// exServer
//
class exServer : public caServer {
public:
    exServer ( const char * const pvPrefix, 
        unsigned aliasCount, bool scanOn );
    ~exServer ();
    void show ( unsigned level ) const;
    pvExistReturn pvExistTest ( const casCtx &, const char * pPVName );
    pvAttachReturn pvAttach ( const casCtx &, const char * pPVName );
    void installAliasName ( pvInfo & info, const char * pAliasName );
    inline void removeAliasName ( pvEntry & entry );
    void removeIO();
private:
    resTable < pvEntry, stringId > stringResTbl;
    unsigned simultAsychIOCount;
    bool scanOn;

    //
    // list of pre-created PVs
    //
    static pvInfo pvList[];
    static const unsigned pvListNElem;

    //
    // on-the-fly PVs 
    //
    static pvInfo bill;
    static pvInfo billy;
    static pvInfo bloaty;
    static pvInfo boot;
    static pvInfo booty;
};

//
// exAsyncPV
//
class exAsyncPV : public exScalarPV {
public:
    exAsyncPV ( pvInfo &setup, bool preCreateFlag, bool scanOnIn );
    caStatus read ( const casCtx & ctxIn, gdd & protoIn );
    caStatus write ( const casCtx & ctxIn, const gdd & value );
    void removeIO();
private:
    unsigned simultAsychIOCount;
};

//
// exChannel
//
class exChannel : public casChannel{
public:
    exChannel ( const casCtx & ctxIn );
    void setOwner ( const char * const pUserName, 
        const char * const pHostName );
    bool readAccess () const;
    bool writeAccess () const;
private:
};

//
// exAsyncWriteIO
//
class exAsyncWriteIO : public casAsyncWriteIO, public epicsTimerNotify {
public:
    exAsyncWriteIO ( const casCtx & ctxIn, exAsyncPV & pvIn, const gdd & valueIn );
    ~exAsyncWriteIO ();
private:
    exAsyncPV & pv;
    epicsTimer & timer;
    smartConstGDDPointer pValue;
    expireStatus expire ( const epicsTime & currentTime );
};

//
// exAsyncReadIO
//
class exAsyncReadIO : public casAsyncReadIO, public epicsTimerNotify {
public:
    exAsyncReadIO ( const casCtx & ctxIn, exAsyncPV & pvIn, gdd & protoIn );
    virtual ~exAsyncReadIO ();
private:
    exAsyncPV & pv;
    epicsTimer & timer;
    smartGDDPointer pProto;
    expireStatus expire ( const epicsTime & currentTime );
};

//
// exAsyncExistIO
// (PV exist async IO)
//
class exAsyncExistIO : public casAsyncPVExistIO, public epicsTimerNotify {
public:
    exAsyncExistIO ( const pvInfo & pviIn, const casCtx & ctxIn,
            exServer & casIn );
    virtual ~exAsyncExistIO ();
private:
    const pvInfo & pvi;
    epicsTimer & timer;
    exServer & cas;
    expireStatus expire ( const epicsTime & currentTime );
};

 
//
// exAsyncCreateIO
// (PV create async IO)
//
class exAsyncCreateIO : public casAsyncPVAttachIO, public epicsTimerNotify {
public:
    exAsyncCreateIO ( pvInfo & pviIn, exServer & casIn, 
        const casCtx & ctxIn, bool scanOnIn );
    virtual ~exAsyncCreateIO ();
private:
    pvInfo & pvi;
    epicsTimer & timer;
    exServer & cas;
    bool scanOn;
    expireStatus expire ( const epicsTime & currentTime );
};

inline pvInfo::pvInfo ( double scanPeriodIn, const char *pNameIn, 
    aitFloat32 hoprIn, aitFloat32 loprIn,
    aitEnum typeIn, excasIoType ioTypeIn, 
    unsigned countIn ) :

    scanPeriod ( scanPeriodIn ), pName ( pNameIn ), 
    hopr ( hoprIn ), lopr ( loprIn ), type ( typeIn ),
    ioType ( ioTypeIn ), elementCount ( countIn ), 
    pPV ( 0 )
{
}

//
// for use when MSVC++ will not build a default copy constructor 
// for this class
//
inline pvInfo::pvInfo ( const pvInfo & copyIn ) :

    scanPeriod ( copyIn.scanPeriod ), pName ( copyIn.pName ), 
    hopr ( copyIn.hopr ), lopr ( copyIn.lopr ), type ( copyIn.type ),
    ioType ( copyIn.ioType ), elementCount ( copyIn.elementCount ),
    pPV ( copyIn.pPV )
{
}

inline pvInfo::~pvInfo ()
{
    //
    // GDD cleanup gets rid of GDD's that are in use 
    // by the PV before the file scope destructer for 
    // this class runs here so this does not seem to 
    // be a good idea
    //
    //if ( this->pPV != NULL ) {
    //   delete this->pPV;
    //}
}

inline void pvInfo::deletePV ()
{
    if ( this->pPV != NULL ) {
        delete this->pPV;
    }
}

inline double pvInfo::getScanPeriod () const 
{ 
    return this->scanPeriod; 
}

inline const char *pvInfo::getName () const 
{ 
    return this->pName; 
}

inline double pvInfo::getHopr () const 
{ 
    return this->hopr; 
}

inline double pvInfo::getLopr () const 
{ 
    return this->lopr; 
}

inline aitEnum pvInfo::getType () const 
{ 
    return this->type;
}

inline excasIoType pvInfo::getIOType () const 
{ 
    return this->ioType; 
}

inline unsigned pvInfo::getElementCount () const 
{ 
    return this->elementCount; 
}

inline void pvInfo::unlinkPV () 
{ 
    this->pPV = NULL; 
}

inline pvEntry::pvEntry ( pvInfo  & infoIn, exServer & casIn, 
        const char * pAliasName ) : 
    stringId ( pAliasName ), info ( infoIn ), cas ( casIn ) 
{
    assert ( this->stringId::resourceName() != NULL );
}

inline pvEntry::~pvEntry ()
{
    this->cas.removeAliasName ( *this );
}

inline void pvEntry::destroy ()
{
    delete this;
}

inline void exServer::removeAliasName ( pvEntry & entry )
{
    pvEntry * pE;
    pE = this->stringResTbl.remove ( entry );
    assert ( pE == &entry );
}

inline double exPV::getScanPeriod ()
{
    double curPeriod = this->info.getScanPeriod ();
    if ( ! this->interest ) {
        curPeriod *= 10.0L;
    }
    return curPeriod;
}

inline caStatus exPV::readNoCtx ( smartGDDPointer pProtoIn )
{
    return this->ft.read ( *this, *pProtoIn );
}

inline const pvInfo & exPV::getPVInfo ()
{
    return this->info;
}

inline const char * exPV::getName () const
{
    return this->info.getName();
}

inline void exServer::removeIO()
{
    if ( this->simultAsychIOCount > 0u ) {
        this->simultAsychIOCount--;
    }
    else {
        fprintf ( stderr, 
        "simultAsychIOCount underflow?\n" );
    }
}

inline exAsyncPV::exAsyncPV ( pvInfo & setup, bool preCreateFlag, 
        bool scanOnIn ) :
    exScalarPV ( setup, preCreateFlag, scanOnIn ),
    simultAsychIOCount ( 0u ) 
{
}

inline void exAsyncPV::removeIO ()
{
    if ( this->simultAsychIOCount > 0u ) {
        this->simultAsychIOCount--;
    }
    else {
        fprintf ( stderr, "inconsistent simultAsychIOCount?\n" );
    }
}

inline exChannel::exChannel ( const casCtx & ctxIn ) : 
    casChannel(ctxIn) 
{
}

