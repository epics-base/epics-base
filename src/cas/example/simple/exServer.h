//
// 	Example EPICS CA server
//
//
// 	caServer
//	|
//	exServer
//
//	casPV
//	|
//	exPV-------------
//	|		|
//	exScalarPV	exVectorPV
//	|
//	exAsyncPV
//
//


//
// ANSI C
//
#include <string.h>
#include <stdio.h>

//
// EPICS
//
#include "epicsAssert.h"
#include "casdef.h"
#include "gddAppFuncTable.h"
#include "osiTimer.h"
#include "resourceLib.h"
#include "tsMinMax.h"

#ifndef NELEMENTS
#	define NELEMENTS(A) (sizeof(A)/sizeof(A[0]))
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
	const unsigned getElementCount() const { return this->elementCount; }
        void destroyPV() { this->pPV=NULL; }
        exPV *createPV (exServer &exCAS, aitBool preCreateFlag);
private:
	const double		scanPeriod;
	const char 		*pName;
	const double 		hopr;
	const double		lopr;
	const excasIoType	ioType;
	const unsigned		elementCount;
	exPV			*pPV;
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
class pvEntry : public stringId, public tsSLNode<pvEntry> {
public:
	pvEntry (pvInfo  &infoIn, exServer &casIn, const char *pAliasName) : 
		stringId(pAliasName), info(infoIn), cas(casIn) 
	{
		assert(this->stringId::resourceName()!=NULL);
	}

	inline ~pvEntry();

	pvInfo &getInfo() const { return this->info; }
	
	void destroy ()
	{
		//
		// always created with new
		//
		delete this;
	}
private:
	pvInfo &info;
	exServer &cas;
};

//
// exScanTimer
//
class exScanTimer : public osiTimer {
public:
	exScanTimer (double delayIn, exPV &pvIn) : 
		pv(pvIn), osiTimer(delayIn) {}
	void expire ();
	osiBool again() const;
	const osiTime delay() const;
	const char *name() const;
private:
	exPV	&pv;
};

//
// exPV
//
class exPV : public casPV, public tsSLNode<exPV> {
public:
	exPV (caServer &cas, pvInfo &setup, aitBool preCreateFlag);
	virtual ~exPV();

	void show(unsigned level) const;

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
	caStatus update (gdd &value);

	//
	// Gets called when we add noise to the current value
	//
	virtual void scan() = 0;

	//
	// Std PV Attribute fetch support
	//
	gddAppFuncTableStatus getStatus(gdd &value);
	gddAppFuncTableStatus getSeverity(gdd &value);
	gddAppFuncTableStatus getSeconds(gdd &value);
	gddAppFuncTableStatus getNanoseconds(gdd &value);
	gddAppFuncTableStatus getPrecision(gdd &value);
	gddAppFuncTableStatus getHighLimit(gdd &value);
	gddAppFuncTableStatus getLowLimit(gdd &value);
	gddAppFuncTableStatus getUnits(gdd &value);
	gddAppFuncTableStatus getValue(gdd &value);
	gddAppFuncTableStatus getEnums(gdd &value);
	
	//
	//
	//
	aitTimeStamp getTS();

        //
	// If no one is watching scan the PV with 10.0
	// times the specified period
        //
	const float getScanPeriod()
	{
		double curPeriod;

		curPeriod = this->info.getScanPeriod();
		if (!this->interest) {
			curPeriod *= 10.0L;
		}
		return curPeriod;
	}

	caStatus read (const casCtx &, gdd &protoIn);

	caStatus write (const casCtx &, gdd &protoIn);

	void destroy();

	const pvInfo &getPVInfo()
	{
		return this->info;
	}

	const char *getName() const
	{
		return this->info.getName();
	}

protected:
	gdd			*pValue;
	exScanTimer		*pScanTimer;
	pvInfo & 		info; 
	aitBool			interest;
	aitBool			preCreate;
	static osiTime		currentTime;

	virtual caStatus updateValue (gdd &value) = 0;
};

//
// exScalerPV
//
class exScalarPV : public exPV {
public:
	exScalarPV (caServer &cas, pvInfo &setup, aitBool preCreateFlag) :
			exPV (cas, setup, preCreateFlag) {}
	void scan();
private:
	caStatus updateValue (gdd &value);
};

//
// exVectorPV
//
class exVectorPV : public exPV {
public:
	exVectorPV (caServer &cas, pvInfo &setup, aitBool preCreateFlag) :
			exPV (cas, setup, preCreateFlag) {}
	void scan();

	unsigned maxDimension() const;
	aitIndex maxBound (unsigned dimension) const;

private:
 	caStatus updateValue (gdd &value);
};

//
// exServer
//
class exServer : public caServer {
public:
	exServer(const char * const pvPrefix, unsigned aliasCount);
        void show (unsigned level) const;
        pvExistReturn pvExistTest (const casCtx&, const char *pPVName);
        pvCreateReturn createPV (const casCtx &ctx, const char *pPVName);

	void installAliasName(pvInfo &info, const char *pAliasName);
	inline void removeAliasName(pvEntry &entry);

	static gddAppFuncTableStatus read(exPV &pv, gdd &value) 
	{
		return exServer::ft.read(pv, value);
	}

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
	resTable<pvEntry,stringId> stringResTbl;
	unsigned simultAsychIOCount;

	//
	// list of pre-created PVs
	//
	static pvInfo pvList[];

	//
	// on-the-fly PVs 
	//
	static pvInfo bill;
	static pvInfo billy;

	static gddAppFuncTable<exPV> ft;
};

//
// exAsyncPV
//
class exAsyncPV : public exScalarPV {
public:
	//
	// exAsyncPV()
	//
	exAsyncPV (caServer &cas, pvInfo &setup, aitBool preCreateFlag) :
		exScalarPV (cas, setup, preCreateFlag),
		simultAsychIOCount(0u) {}

        //
        // read
        //
        caStatus read(const casCtx &ctxIn, gdd &value);

        //
        // write
        //
        caStatus write(const casCtx &ctxIn, gdd &value);

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

        //void setOwner(const char *pUserName, const char *pHostName){};

        //
        // called when the first client begins to monitor the PV
        //
        caStatus interestRegister () 
	{	
		return S_cas_success;
	}

        //
        // called when the last client stops monitoring the PV
        //
        void interestDelete () {}

        //
        // the following are encouraged to change during an channel's
        // lifetime
        //
        aitBool readAccess () const {return aitTrue;};
        aitBool writeAccess () const {return aitTrue;};

private:
};

//
// exOSITimer
//
// a special version of osiTimer which is only to be used 
// within an exAsyncIO. The destroy() method is replaced 
// so that the timer destroy() will not destroy the
// exAsyncIO until the casAsyncIO has completed
//
class exOSITimer : public osiTimer {
public:
	exOSITimer() : osiTimer(osiTime(0.010)) {} // 10 mSec

	//
	// this is a noop that postpones the timer expiration
	// destroy so this object will hang around until the
	// casAsyncIO::destroy() is called
	//
	void destroy();
};

//
// exAsyncWriteIO
//
class exAsyncWriteIO : public casAsyncWriteIO, public exOSITimer {
public:
	//
	// exAsyncWriteIO() 
	//
	exAsyncWriteIO(const casCtx &ctxIn, exAsyncPV &pvIn, gdd &valueIn) :
		casAsyncWriteIO(ctxIn), pv(pvIn), value(valueIn) 
	{
		this->value.reference();
	}

	~exAsyncWriteIO()
	{
		this->pv.removeIO();
		this->value.unreference();
	}

	//
	// expire()
	// (a virtual function that runs when the base timer expires)
	// see exAsyncPV.cc
	//
	void expire();

	const char *name() const;

private:
	exAsyncPV	&pv;
	gdd		&value;
};

//
// exAsyncReadIO
//
class exAsyncReadIO : public casAsyncReadIO, public exOSITimer {
public:
	//
	// exAsyncReadIO()
	//
	exAsyncReadIO(const casCtx &ctxIn, exAsyncPV &pvIn, gdd &protoIn) :
		casAsyncReadIO(ctxIn), pv(pvIn), proto(protoIn) 
	{
		this->proto.reference();
	}

	~exAsyncReadIO()
	{
		this->pv.removeIO();
		this->proto.unreference();
	}

	//
	// expire()
	// (a virtual function that runs when the base timer expires)
	// see exAsyncPV.cc
	//
	void expire();
		
	const char *name() const;

private:
	exAsyncPV	&pv;
	gdd		&proto;
};

//
// exAsyncExistIO
// (PV exist async IO)
//
class exAsyncExistIO : public casAsyncPVExistIO, public exOSITimer {
public:
	//
	// exAsyncExistIO()
	//
	exAsyncExistIO(const pvInfo &pviIn, const casCtx &ctxIn,
			exServer &casIn) :
		casAsyncPVExistIO(ctxIn), pvi(pviIn), cas(casIn) {}

	~exAsyncExistIO()
	{
		this->cas.removeIO();
	}

	//
	// expire()
	// (a virtual function that runs when the base timer expires)
	// see exServer.cc
	//
	void expire();

	const char *name() const;
private:
	const pvInfo	&pvi;
	exServer	&cas;
};

 
//
// exAsyncCreateIO
// (PV create async IO)
//
class exAsyncCreateIO : public casAsyncPVCreateIO, public exOSITimer {
public:
	//
	// exAsyncCreateIO()
	//
	exAsyncCreateIO(pvInfo &pviIn, exServer &casIn, 
		const casCtx &ctxIn) :
		casAsyncPVCreateIO(ctxIn), pvi(pviIn), cas(casIn) {}

	~exAsyncCreateIO()
	{
		this->cas.removeIO();
	}

	//
	// expire()
	// (a virtual function that runs when the base timer expires)
	// see exServer.cc
	//
	void expire();

	const char *name() const;
private:
	pvInfo		&pvi;
	exServer	&cas;
};

//
// exServer::removeAliasName()
//
inline void exServer::removeAliasName(pvEntry &entry)
{
        pvEntry *pE;
        pE = this->stringResTbl.remove(entry);
        assert(pE = &entry);
}

//
// pvEntry::~pvEntry()
//
inline pvEntry::~pvEntry()
{
        this->cas.removeAliasName(*this);
}
 
