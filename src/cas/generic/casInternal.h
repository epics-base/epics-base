/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 */

//
// EPICS
//
#include "tsDLList.h"
#include "resourceLib.h"
#include "caProto.h"
#include "smartGDDPointer.h"

typedef aitUint32 caResId;

class casEventSys;
class casChannelI;

//
// casEvent
//
class casEvent : public tsDLNode<casEvent> {
public:
    virtual ~casEvent();
    virtual caStatus cbFunc (casEventSys &)=0;
private:
};

class casChanDelEv : public casEvent {
public:
	casChanDelEv(caResId idIn) : id(idIn) {}
	~casChanDelEv();
	caStatus cbFunc(casEventSys &);
private:
	caResId	id;
};

enum casResType {casChanT=1, casClientMonT, casPVT};

class casRes : public chronIntIdRes<casRes>
{
public:
	virtual ~casRes();
	virtual casResType resourceType() const = 0;
	virtual void show (unsigned level) const = 0;
	virtual void destroy() = 0;
private:
};

class ioBlockedList;
class osiMutex;

//
// ioBlocked
//
class ioBlocked : public tsDLNode<ioBlocked> {
friend class ioBlockedList;
public:
	ioBlocked ();
	virtual ~ioBlocked ();
private:
	ioBlockedList	*pList;
	virtual void ioBlockedSignal () = 0;
};

//
// ioBlockedList
//
class ioBlockedList : private tsDLList<ioBlocked> {
public:
	ioBlockedList ();
	virtual ~ioBlockedList ();
	void signal ();
	void removeItemFromIOBLockedList(ioBlocked &item);
	void addItemToIOBLockedList(ioBlocked &item);
};
 
class casMonitor;

//
// casMonEvent
//
class casMonEvent : public casEvent {
public:
	//
	// only used when this is part of another structure
	// (and we need to postpone true construction)
	//
	inline casMonEvent ();
	inline casMonEvent (casMonitor &monitor, gdd &newValue);
	inline casMonEvent (const casMonEvent &initValue);

	//
	// ~casMonEvent ()
	// (not inline because this is virtual in the base class)
	//
	~casMonEvent ();

	caStatus cbFunc(casEventSys &);

	//
	// casMonEvent::getValue()
	//
	inline gdd *getValue() const;

	inline void operator = (const class casMonEvent &monEventIn);
	
	inline void clear();

	void assign (casMonitor &monitor, gdd *pValueIn);
private:
	smartGDDPointer pValue;
	caResId id;
};


//
// casMonitor()
//
class casMonitor : public tsDLNode<casMonitor>, public casRes {
public:
	casMonitor(caResId clientIdIn, casChannelI &chan, 
	unsigned long nElem, unsigned dbrType,
	const casEventMask &maskIn, osiMutex &mutexIn);
	virtual ~casMonitor();

	caStatus executeEvent(casMonEvent *);

	inline void post(const casEventMask &select, gdd &value);

	virtual void show (unsigned level) const;
	virtual caStatus callBack(gdd &value)=0;

	caResId getClientId() const 
	{
		return this->clientId;
	}

	unsigned getType() const 
	{
		return this->dbrType;
	}

	unsigned long getCount() const 
	{
		return this->nElem;
	}
	casChannelI &getChannel() const 
	{	
		return this->ciu;
	}

private:
	casMonEvent overFlowEvent;
	unsigned long const nElem;
	osiMutex &mutex;
	casChannelI &ciu;
	const casEventMask mask;
	caResId const clientId;
	unsigned char const dbrType;
	unsigned char nPend;
	unsigned ovf:1;
	unsigned enabled:1;

	void enable();
	void disable();

	void push (gdd &value);
};

//
// casMonitor::post()
// (check for NOOP case in line)
//
inline void casMonitor::post(const casEventMask &select, gdd &value)
{
	casEventMask	result(select&this->mask);
	//
	// NOOP if this event isnt selected
	// or if it is disabled
	//
	if (result.noEventsSelected() || !this->enabled) {
		return;
	}

	//
	// else push it on the queue
	//
	this->push(value);
}

class caServer;
class casCoreClient;
class casChannelI;
class casCtx;
class caServer;
class casAsyncReadIO;
class casAsyncWriteIO;
class casAsyncPVExistIO;
class casAsyncPVAttachIO;

class casAsyncIOI : public casEvent, public tsDLNode<casAsyncIOI> {
public:
	casAsyncIOI (casCoreClient &client);
	virtual ~casAsyncIOI();

	void serverDestroyIfReadOP ();
    void serverDestroy ();

	caServer *getCAS() const;

protected:
	casCoreClient &client;

	//
	// place notification of IO completion on the event queue
	//
	caStatus postIOCompletionI();

	void lock();
	void unlock();

private:
	unsigned inTheEventQueue:1;
	unsigned posted:1;
	unsigned ioComplete:1;
	unsigned serverDelete:1;
	unsigned duplicate:1;

	//
	// casEvent virtual call back function
	// (called when IO completion event reaches top of event queue)
	//
	epicsShareFunc caStatus cbFunc(casEventSys &);

    //
    // derived class specic call back
	// (called when IO completion event reaches top of event queue)
    //
    epicsShareFunc virtual caStatus cbFuncAsyncIO() = 0;
    epicsShareFunc virtual void destroy ();

	epicsShareFunc virtual bool readOP() const;
};

class casDGClient;

class casChannel;
class casPVI;

//
// casChannelI
//
// this derives from casEvent so that access rights
// events can be posted
//
class casChannelI : public tsDLNode<casChannelI>, public casRes, 
				public casEvent {
public:
	casChannelI (const casCtx &ctx);
	virtual ~casChannelI();

	casCoreClient &getClient () const
	{	
		return this->client;
	}
	const caResId getCID () 
	{
		return this->cid;
	}
	//
	// fetch the unsigned integer server id for this PV
	//
	const caResId getSID ();

	//
	// addMonitor()
	//
	void addMonitor (casMonitor &mon);

	//
	// deleteMonitor()
	//
	void deleteMonitor (casMonitor &mon);

	//
	// findMonitor
	// (it is reasonable to do a linear search here because
	// sane clients will require only one or two monitors 
	// per channel)
	//
	casMonitor *findMonitor (const caResId clientIdIn);

	casPVI &getPVI () const 
	{
		return this->pv;
	}

	void installAsyncIO (casAsyncIOI &);
	void removeAsyncIO (casAsyncIOI &);

	void postEvent (const casEventMask &select, gdd &event);

	epicsShareFunc virtual casResType resourceType () const;

	void lock () const;
	void unlock () const;

	void destroyNoClientNotify ();
    void destroyClientNotify ();

	void clearOutstandingReads ();

	//
	// access rights event call back
	//
	epicsShareFunc caStatus cbFunc (casEventSys &);

	void postAccessRightsEvent ();

    //
    // virtual functions
    //
	epicsShareFunc virtual void setOwner (const char * const pUserName, 
		const char * const pHostName) = 0;
	epicsShareFunc virtual bool readAccess () const = 0;
	epicsShareFunc virtual bool writeAccess () const = 0;
	epicsShareFunc virtual bool confirmationRequested () const = 0;
	epicsShareFunc virtual void show (unsigned level) const;

protected:
	tsDLList<casMonitor>    monitorList;
	tsDLList<casAsyncIOI>   ioInProgList;
	casCoreClient           &client;
	casPVI                  &pv;
	caResId const           cid;    // client id 
	unsigned                accessRightsEvPending:1;

	epicsShareFunc virtual void destroy ();
};

//
// class hierarchy added here solely because we need two list nodes
//
class casPVListChan : public casChannelI, public tsDLNode<casPVListChan>
{
public:
        casPVListChan (const casCtx &ctx);
        virtual ~casPVListChan();
};

class caServerI;
class casCtx;
class casChannel;
class casPV;

//
// casPVI
//
class casPVI : 
public tsSLNode<casPVI>,  // server resource table installation 
public casRes,		// server resource table installation 
public ioBlockedList	// list of clients io blocked on this pv
{
public:
    casPVI ();
    virtual ~casPVI (); 
    
    //
    // for use by the server library
    //
    caServerI *getPCAS () const;
    
    //
    // attach to a server
    //
    caStatus attachToServer (caServerI &cas);
    
    //
    // CA only does 1D arrays for now (and the new server
    // temporarily does only scalars)
    //
    aitIndex nativeCount ();
    
    //
    // only for use by casMonitor
    //
    caStatus registerEvent ();
    void unregisterEvent ();
    
    //
    // only for use by casAsyncIOI 
    //
    void unregisterIO ();
    
    //
    // only for use by casChannelI
    //
    void installChannel (casPVListChan &chan);
    
    //
    // only for use by casChannelI
    //
    void removeChannel (casPVListChan &chan);
    
    //
    // check for none attached and delete self if so
    //
    void deleteSignal ();
    
    void postEvent (const casEventMask &select, gdd &event);
    
    caServer *getExtServer () const;
    
    //
    // bestDBRType()
    //
    caStatus bestDBRType (unsigned &dbrType);
       
    epicsShareFunc virtual casResType resourceType () const;
    
    //
    // virtual functions in the public interface class
    //
    epicsShareFunc virtual void show (unsigned level) const;
    epicsShareFunc virtual caStatus interestRegister () = 0;
    epicsShareFunc virtual void interestDelete () = 0;
    epicsShareFunc virtual caStatus beginTransaction () = 0;
    epicsShareFunc virtual void endTransaction () = 0;
    epicsShareFunc virtual caStatus read (const casCtx &ctx, gdd &prototype) = 0;
    epicsShareFunc virtual caStatus write (const casCtx &ctx, gdd &value) = 0;
    epicsShareFunc virtual casChannel *createChannel (const casCtx &ctx,
        const char * const pUserName, const char * const pHostName) = 0;
    epicsShareFunc virtual aitEnum bestExternalType () const = 0;
    epicsShareFunc virtual unsigned maxDimension () const = 0; 
    epicsShareFunc virtual aitIndex maxBound (unsigned dimension) const = 0;
    epicsShareFunc virtual const char *getName () const = 0;
    epicsShareFunc casPV *apiPointer (); //retuns NULL if casPVI isnt a base of casPV

private:
    tsDLList<casPVListChan> chanList;
    caServerI               *pCAS;
    unsigned                nMonAttached;
    unsigned                nIOAttached;
   
    inline void lock () const;
    inline void unlock () const;


    epicsShareFunc virtual void destroy (); // casPVI destructor noop
};

