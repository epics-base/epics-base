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
enum excasIoType {excasIoSync, excasIoAsync};

class exPV;
class exServer;

//
// pvInfo
//
class pvInfo {
public:
    pvInfo (double scanPeriodIn, const char *pNameIn, 
            aitFloat32 hoprIn, aitFloat32 loprIn, 
            excasIoType ioTypeIn, unsigned countIn) :
            scanPeriod(scanPeriodIn), pName(pNameIn), 
            hopr(hoprIn), lopr(loprIn), ioType(ioTypeIn), 
            elementCount(countIn), pPV(0)
    {
    }

    ~pvInfo ();

    //
    // for use when MSVC++ will not build a default copy constructor 
    // for this class
    //
    pvInfo (const pvInfo &copyIn) :
        scanPeriod(copyIn.scanPeriod), pName(copyIn.pName), 
        hopr(copyIn.hopr), lopr(copyIn.lopr), 
        ioType(copyIn.ioType), elementCount(copyIn.elementCount),
        pPV(copyIn.pPV)
    {
    }

    const double getScanPeriod () const { return this->scanPeriod; }
    const char *getName () const { return this->pName; }
    const double getHopr () const { return this->hopr; }
    const double getLopr () const { return this->lopr; }
    const excasIoType getIOType () const { return this->ioType; }
    const unsigned getElementCount() const 
        { return this->elementCount; }
    void unlinkPV() { this->pPV=NULL; }

    exPV *createPV (exServer &exCAS, bool preCreateFlag, bool scanOn);
    void deletePV ();

private:
    const double    scanPeriod;
    const char      *pName;
    const double    hopr;
    const double    lopr;
    const excasIoType   ioType;
    const unsigned  elementCount;
    exPV            *pPV;
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
    : public stringId, public tsSLNode<pvEntry> {
public:
    pvEntry (pvInfo  &infoIn, exServer &casIn, 
            const char *pAliasName) : 
        stringId(pAliasName), info(infoIn), cas(casIn) 
    {
        assert(this->stringId::resourceName()!=NULL);
    }

    inline ~pvEntry();

    pvInfo &getInfo() const { return this->info; }
    
    inline void destroy ();

private:
    pvInfo &info;
    exServer &cas;
};


//
// exPV
//
class exPV : public casPV, public epicsTimerNotify, public tsSLNode<exPV> {
public:
    exPV ( pvInfo &setup, bool preCreateFlag, bool scanOn );
    virtual ~exPV();

    void show ( unsigned level ) const;

    //
    // Called by the server libary each time that it wishes to
    // subscribe for PV the server tool via postEvent() below.
    //
    caStatus interestRegister();

    //
    // called by the server library each time that it wishes to
    // remove its subscription for PV value change events
    // from the server tool via caServerPostEvents()
    //
    void interestDelete();

    aitEnum bestExternalType() const;

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
    caStatus update (smartConstGDDPointer pValue);

    //
    // Gets called when we add noise to the current value
    //
    virtual void scan() = 0;
    
    //
    // If no one is watching scan the PV with 10.0
    // times the specified period
    //
    const double getScanPeriod()
    {
        double curPeriod;

        curPeriod = this->info.getScanPeriod ();
        if ( ! this->interest ) {
            curPeriod *= 10.0L;
        }
        return curPeriod;
    }

    caStatus read (const casCtx &, gdd &protoIn);

    caStatus readNoCtx (smartGDDPointer pProtoIn)
    {
        return this->ft.read (*this, *pProtoIn);
    }

    caStatus write (const casCtx &, const gdd &value);

    void destroy();

    const pvInfo &getPVInfo()
    {
        return this->info;
    }

    const char *getName() const
    {
        return this->info.getName();
    }

    static void initFT();

    //
    // for access control - optional
    //
    casChannel *createChannel (const casCtx &ctx,
        const char * const pUserName, const char * const pHostName);

protected:
    smartConstGDDPointer    pValue;
    epicsTimer              &timer;
    pvInfo &                info; 
    bool                    interest;
    bool                    preCreate;
    bool                    scanOn;
    static epicsTime        currentTime;

    virtual caStatus updateValue (smartConstGDDPointer pValue) = 0;

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
    ~exServer();

    void show (unsigned level) const;
    pvExistReturn pvExistTest (const casCtx&, const char *pPVName);
    pvAttachReturn pvAttach (const casCtx &ctx, const char *pPVName);

    void installAliasName(pvInfo &info, const char *pAliasName);
    inline void removeAliasName(pvEntry &entry);

    //
    // removeIO
    //
    void removeIO()
    {
        if (this->simultAsychIOCount>0u) {
            this->simultAsychIOCount--;
        }
        else {
            fprintf(stderr, 
            "simultAsychIOCount underflow?\n");
        }
    }
private:
    resTable<pvEntry,stringId> stringResTbl;
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
};

//
// exAsyncPV
//
class exAsyncPV : public exScalarPV {
public:
    //
    // exAsyncPV()
    //
    exAsyncPV ( pvInfo &setup, bool preCreateFlag, bool scanOnIn ) :
        exScalarPV ( setup, preCreateFlag, scanOnIn ),
        simultAsychIOCount ( 0u ) {}

    //
    // read
    //
    caStatus read (const casCtx &ctxIn, gdd &protoIn);

    //
    // write
    //
    caStatus write (const casCtx &ctxIn, const gdd &value);

    //
    // removeIO
    //
    void removeIO()
    {
        if (this->simultAsychIOCount>0u) {
            this->simultAsychIOCount--;
        }
        else {
            fprintf(stderr, "inconsistent simultAsychIOCount?\n");
        }
    }
private:
    unsigned simultAsychIOCount;
};

//
// exChannel
//
class exChannel : public casChannel{
public:
    exChannel(const casCtx &ctxIn) : casChannel(ctxIn) {}

    virtual void setOwner(const char * const pUserName, 
        const char * const pHostName);

    virtual bool readAccess () const;
    virtual bool writeAccess () const;

private:
};

//
// exAsyncWriteIO
//
class exAsyncWriteIO : public casAsyncWriteIO, public epicsTimerNotify {
public:
    exAsyncWriteIO ( const casCtx &ctxIn, exAsyncPV &pvIn, const gdd &valueIn );
    virtual ~exAsyncWriteIO ();
private:
    exAsyncPV &pv;
    epicsTimer &timer;
    smartConstGDDPointer pValue;
    expireStatus expire ( const epicsTime & currentTime );
};

//
// exAsyncReadIO
//
class exAsyncReadIO : public casAsyncReadIO, public epicsTimerNotify {
public:
    exAsyncReadIO ( const casCtx &ctxIn, exAsyncPV &pvIn, gdd &protoIn );
    virtual ~exAsyncReadIO ();
private:
    exAsyncPV &pv;
    epicsTimer &timer;
    smartGDDPointer pProto;
    expireStatus expire ( const epicsTime & currentTime );
};

//
// exAsyncExistIO
// (PV exist async IO)
//
class exAsyncExistIO : public casAsyncPVExistIO, public epicsTimerNotify {
public:
    exAsyncExistIO ( const pvInfo &pviIn, const casCtx &ctxIn,
            exServer &casIn );
    virtual ~exAsyncExistIO ();
private:
    const pvInfo &pvi;
    epicsTimer &timer;
    exServer &cas;
    expireStatus expire ( const epicsTime & currentTime );
};

 
//
// exAsyncCreateIO
// (PV create async IO)
//
class exAsyncCreateIO : public casAsyncPVAttachIO, public epicsTimerNotify {
public:
    exAsyncCreateIO ( pvInfo &pviIn, exServer &casIn, 
        const casCtx &ctxIn, bool scanOnIn );
    virtual ~exAsyncCreateIO ();
private:
    pvInfo      &pvi;
    epicsTimer  &timer;
    exServer    &cas;
    bool        scanOn;
    expireStatus expire ( const epicsTime & currentTime );
};

//
// exServer::removeAliasName()
//
inline void exServer::removeAliasName ( pvEntry &entry )
{
        pvEntry *pE;
        pE = this->stringResTbl.remove ( entry );
        assert ( pE == &entry );
}

//
// pvEntry::~pvEntry()
//
inline pvEntry::~pvEntry()
{
    this->cas.removeAliasName ( *this );
}

//
// pvEntry:: destroy()
//
inline void pvEntry::destroy ()
{
    delete this;
}

inline pvInfo::~pvInfo ()
{
    if ( this->pPV != NULL ) {
        delete this->pPV;
    }
}

inline void pvInfo::deletePV ()
{
    if ( this->pPV != NULL ) {
        delete this->pPV;
    }
}

