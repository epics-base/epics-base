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
#include <epicsAssert.h>
#include <casdef.h>
#include <gddAppFuncTable.h>
#include <osiTimer.h>

#ifndef max
#define max(A,B) ((A)<(B)?(B):(A))
#endif

#ifndef min
#define min(A,B) ((A)>(B)?(B):(A))
#endif

#ifndef NELEMENTS
#	define NELEMENTS(A) (sizeof(A)/sizeof(A[0]))
#endif

#define LOCAL static

//
// info about all pv in this server
//
enum excasIoType {excasIoSync, excasIoAsync};

class pvInfo {
public:
	pvInfo (double scanRateIn, const char *pName, 
			aitFloat32 hoprIn, aitFloat32 loprIn, 
			excasIoType ioTypeIn, unsigned countIn) :
			scanRate(scanRateIn), name(pName), hopr(hoprIn), 
			lopr(loprIn), ioType(ioTypeIn), elementCount(countIn)
	{
	}

	//
	// for use when MSVC++ will not build a default copy constructor 
	// for this class
	//
	pvInfo (const pvInfo &copyIn) :
			scanRate(copyIn.scanRate), name(copyIn.name), 
			hopr(copyIn.hopr), lopr(copyIn.lopr), 
			ioType(copyIn.ioType), elementCount(copyIn.elementCount)
	{
	}

	const double getScanRate () const { return this->scanRate; }
	const aitString &getName () const { return this->name; }
	const double getHopr () const { return this->hopr; }
	const double getLopr () const { return this->lopr; }
	const excasIoType getIOType () const { return this->ioType; }
	const unsigned getElementCount() const { return this->elementCount; }
private:
	const double		scanRate;
	const aitString		name;
	const double 		hopr;
	const double		lopr;
	const excasIoType	ioType;
	const unsigned		elementCount;
};

class exPV;


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
class exPV : public casPV {

public:
	exPV (const casCtx &ctxIn, const pvInfo &setup);
	virtual ~exPV();

	void show(unsigned level);

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

	const float getScanRate()
	{
		return this->info.getScanRate();
	}

	caStatus read (const casCtx &, gdd &protoIn);

	caStatus write (const casCtx &, gdd &protoIn);

protected:
	gdd			*pValue;
	exScanTimer		*pScanTimer;
	const pvInfo & 		info; 
	aitBool			interest;
	static osiTime		currentTime;

	virtual caStatus updateValue (gdd &value) = 0;
};

//
// exScalerPV
//
class exScalarPV : public exPV {
public:
	exScalarPV (const casCtx &ctxIn, const pvInfo &setup) :
			exPV (ctxIn, setup) {}
	void scan();
private:
	caStatus updateValue (gdd &value);
};

//
// exVectorPV
//
class exVectorPV : public exPV {
public:
	exVectorPV (const casCtx &ctxIn, const pvInfo &setup) :
			exPV (ctxIn, setup) {}
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
	exServer(unsigned pvMaxNameLength, unsigned pvCountEstimate=0x3ff,
			unsigned maxSimultaneousIO=1u);
        void show (unsigned level);
        pvExistReturn pvExistTest (const casCtx &ctxIn, const char *pPVName);
        casPV *createPV (const casCtx &ctxIn, const char *pPVName);

	static const pvInfo *findPV(const char *pName);

	static gddAppFuncTableStatus read(exPV &pv, gdd &value) 
	{
		return exServer::ft.read(pv, value);
	}
private:
	static const pvInfo pvList[];
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
	exAsyncPV (const casCtx &ctxIn, const pvInfo &setup) :
		exScalarPV (ctxIn, setup) {}

        //
        // read
        //
        caStatus read(const casCtx &ctxIn, gdd &value);

        //
        // write
        //
        caStatus write(const casCtx &ctxIn, gdd &value);

	unsigned maxSimultAsyncOps () const;
private:
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
	exAsyncExistIO(const pvInfo &pviIn, const casCtx &ctxIn) :
		casAsyncPVExistIO(ctxIn), pvi(pviIn) {}

	//
	// expire()
	// (a virtual function that runs when the base timer expires)
	// see exServer.cc
	//
	void expire();

	const char *name() const;
private:
	const pvInfo	&pvi;
};

 
