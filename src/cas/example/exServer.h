//
// Example EPICS CA server
//

//
// ANSI C
//
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

//
// SUN C++ does not have RAND_MAX yet 
//
#if !defined(RAND_MAX)
//
// Apparently SUN C++ is using the SYSV version of rand
//
#if 0
#define RAND_MAX INT_MAX 
#else
#define RAND_MAX SHRT_MAX 
#endif
#endif

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
			excasIoType ioTypeIn) :
			scanRate(scanRateIn), name(pName), hopr(hoprIn), 
			lopr(loprIn), ioType(ioTypeIn)
	{
	}

	const double getScanRate () const { return this->scanRate; }
	const aitString &getName () const { return this->name; }
	const double getHopr () const { return this->hopr; }
	const double getLopr () const { return this->lopr; }
	const excasIoType getIOType () const { return this->ioType; }
private:
	const double		scanRate;
	const aitString		name;
	const double 		hopr;
	const double		lopr;
	const excasIoType	ioType;
};

class exPV;

//
// exServer
//
class exServer : public caServer {
public:
	exServer(unsigned pvMaxNameLength, unsigned pvCountEstimate=0x3ff,
			unsigned maxSimultaneousIO=1u);
        void show (unsigned level);
        caStatus pvExistTest (const casCtx &ctxIn, const char *pPVName, 
				gdd &canonicalPVName);
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
// exScanTimer
//
class exScanTimer : public osiTimer {
public:
	exScanTimer (double delayIn, exPV &pvIn) : 
		pv(pvIn), osiTimer(delayIn) {}
	void expire ();
	osiBool again();
	const osiTime delay();
	const char *name()
	{
		return "exScanTimer";
	}
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

	void scanPV();

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

	aitEnum bestExternalType();

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

	caStatus update (gdd &value);

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
	
	//
	//
	//
	aitTimeStamp getTS();

	const float getScanRate()
	{
		return this->info.getScanRate();
	}
protected:
	gdd			*pValue;
	exScanTimer		*pScanTimer;
	const pvInfo & 		info; 
	aitBool			interest;
private:
	static osiTime		currentTime;
};

//
// exSyncPV
//
class exSyncPV : public exPV {
public:
	exSyncPV (const casCtx &ctxIn, const pvInfo &setup);
	~exSyncPV();

        //
        // read
        //
        // this is allowed to complete asychronously
        //
        caStatus read(const casCtx &ctxIn, gdd &value);

        //
        // write
        //
        // this is allowed to complete asychronously
        //
        caStatus write(const casCtx &ctxIn, gdd &value);
private:
};

//
// exAsyncPV
//
class exAsyncPV : public exPV {
public:
	//
	// exAsyncPV()
	//
	exAsyncPV (const casCtx &ctxIn, const pvInfo &setup) : 
		exPV (ctxIn, setup) {}

        //
        // read
        //
        // this is allowed to complete asychronously
        //
        caStatus read(const casCtx &ctxIn, gdd &value);

        //
        // write
        //
        // this is allowed to complete asychronously
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
// exAsyncIOTimer 
//
class exAsyncIOTimer : public osiTimer {
public:
	exAsyncIOTimer (osiTime delayIn) : osiTimer(delayIn) {}
	//
	// this is a noop that postpones the timer expiration
	// destroy so this object will hang around until the
	// casAsyncIO::destroy() is called
	//
	void destroy();

	const char *name()
	{
		return "exAsyncIOTimer";
	}
};

//
// exAsyncIO()
//
class exAsyncIO : public casAsyncIO, public exAsyncIOTimer {
public:
	exAsyncIO(const casCtx &ctxIn, gdd *pValue = 0) :
		casAsyncIO(ctxIn, pValue),
		exAsyncIOTimer(osiTime(0.010)) {} // 10 mSec 
};

//
// exAsyncWriteIO
//
class exAsyncWriteIO : public exAsyncIO {
public:
	//
	// exAsyncWriteIO() 
	//
	exAsyncWriteIO(const casCtx &ctxIn, exAsyncPV &pvIn, gdd &valueIn) :
		exAsyncIO(ctxIn, &valueIn), pv(pvIn) {}

	//
	// expire()
	// (a virtual function that runs when the base timer expires)
	// see exAsyncPV.cc
	//
	void expire();
private:
	exAsyncPV	&pv;
};

//
// exAsyncReadIO
//
class exAsyncReadIO : public exAsyncIO {
public:
	//
	// exAsyncReadIO()
	//
	exAsyncReadIO(const casCtx &ctxIn, exAsyncPV &pvIn, gdd &protoIn) :
		exAsyncIO(ctxIn, &protoIn), pv(pvIn) {}

	//
	// expire()
	// (a virtual function that runs when the base timer expires)
	// see exAsyncPV.cc
	//
	void expire();
		
private:
	exAsyncPV	&pv;
};

//
// exAsyncExistIO
// (PV exist async IO)
//
class exAsyncExistIO : public exAsyncIO {
public:
	//
	// exAsyncExistIO()
	//
	exAsyncExistIO(const pvInfo *pPVI, 
		const casCtx &ctxIn, gdd &canonicalPVName) :
		exAsyncIO(ctxIn, &canonicalPVName)
	{
		canonicalPVName.put (pPVI->getName());
	}

	//
	// expire()
	// (a virtual function that runs when the base timer expires)
	// see exServer.cc
	//
	void expire();
private:
};

 
