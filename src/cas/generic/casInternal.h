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
 * History
 * $Log$
 *
 */

//
// EPICS
//
#include <tsDLList.h>
#include <resourceLib.h>
#include <caProto.h>

typedef aitUint32 caResId;

class casEventSys;
class casChannelI;

//
// casEvent
//
class casEvent : public tsDLNode<casEvent> {
public:
        virtual ~casEvent() {}
        virtual caStatus cbFunc(casEventSys &)=0;
private:
};

class casChanDelEv : public casEvent {
public:
	casChanDelEv(caResId idIn) : id(idIn) {}
        caStatus cbFunc(casEventSys &);
private:
	caResId	id;
};

enum casResType {casChanT=1, casClientMonT, casPVT};
 
class casRes : public uintRes<casRes>
{
public:
        virtual ~casRes() {}
        virtual casResType resourceType() const = 0;
	virtual void show (unsigned level) = 0;
private:
};

//
// ioBlocked
//
class ioBlocked : public tsDLNode<ioBlocked> {
friend class ioBlockedList;
public:
	ioBlocked ();
	virtual ~ioBlocked ();
	void setBlocked (ioBlockedList &list);
private:
	ioBlockedList	*pList;
	virtual void ioBlockedSignal () = 0;
};

//
// ioBlockedList
//
class ioBlockedList : public tsDLList<ioBlocked> {
friend class ioBlocked;
public:
	~ioBlockedList ();
	void signal ();
};


//
// ioBlockedList::~ioBlockedList ()
//
inline ioBlockedList::~ioBlockedList ()
{
	ioBlocked *pB;
	while ( (pB = this->get ()) ) {
		pB->pList = NULL;
	}
}

//
// ioBlockedList::signal ()
//
// works from a temporary list to avoid problems
// where the virtual function adds ites to the 
// list
//
inline void ioBlockedList::signal ()
{
	tsDLList<ioBlocked> tmp(*this);
	ioBlocked *pB;

	while ( (pB = tmp.get ()) ) {
		pB->pList = NULL;
		pB->ioBlockedSignal ();
	}
}

//
// ioBlocked::ioBlocked ()
//
inline ioBlocked::ioBlocked() : pList (NULL) 
{
}

//
// ioBlocked::~ioBlocked ()
//
inline ioBlocked::~ioBlocked ()
{
	if (this->pList) {
		this->pList->remove (*this);
		this->pList = NULL;
	}
}

//
// ioBlocked::setBlocked ()
// 
inline void ioBlocked::setBlocked (ioBlockedList &list) 
{
	if (!this->pList) {
		this->pList = &list;
		list.add (*this);
	}
	else {
		//
		// requests to be in more than one
		// list at at time are fatal
		//
		assert (&list == this->pList);
	}
}

class casMonitor;

//
// casMonEvent
//
class casMonEvent : public casEvent {
public:
	//
	// only used when this part of another structure
	// (and we need to postpone true construction)
	//
	casMonEvent () : pValue(NULL), id(0u) {}
	casMonEvent (casMonitor &monitor, gdd &newValue);
	casMonEvent (casMonEvent &initValue);

	//
	// ~casMonEvent ()
	//
	~casMonEvent ();

	caStatus cbFunc(casEventSys &);

	//
	// casMonEvent::getValue()
	//
	gdd *getValue() const
	{
		return this->pValue;
	}

	void operator = (class casMonEvent &monEventIn)
	{
		int gddStatus;
		if (this->pValue) {
			gddStatus = this->pValue->Unreference();
			assert (!gddStatus);
		}
		if (monEventIn.pValue) {
			gddStatus = monEventIn.pValue->Reference();
			assert (!gddStatus);
		}
		this->pValue = monEventIn.pValue;
		this->id = monEventIn.id;
	}
	
	void clear()
	{
		int gddStatus;
		if (this->pValue) {
			gddStatus = this->pValue->Unreference();
			assert (!gddStatus);
			this->pValue = NULL;
		}
		this->id = 0u;
	}

	void assign (casMonitor &monitor, gdd *pValueIn);
private:
	gdd		*pValue;
	caResId		id;
};

class osiMutex;

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

        void post(const casEventMask &select, gdd &value);

        virtual void show (unsigned level);
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
	
	void postIfModified();

private:
        casMonEvent		overFlowEvent;
	unsigned long const     nElem;
	osiMutex		&mutex;
	casChannelI		&ciu;
	const casEventMask	mask;
	gdd			*pModifiedValue;
	caResId const		clientId;
	unsigned char const	dbrType;
	unsigned char		nPend;
	unsigned		ovf:1;
	unsigned		enabled:1;

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
	//
        if (result.noEventsSelected()) {
                return;
        }
 
        //
        // NOOP if currently disabled
        //
        if (!this->enabled) {
                return;
        }

	//
	// else push it on the queue
	//
	this->push(value);
}


class casCoreClient;
class casAsyncIO;
class casChannelI;
class casCtx;
class caServer;

//
// casAsyncIOI
//
// (server internal asynchronous IO class)
//
// 
// Operations may be completed asynchronously
// by the server tool if the virtual function creates a
// casAsyncIO object and returns the status code
// S_casApp_asyncCompletion
//
class casAsyncIOI : public casEvent, public tsDLNode<casAsyncIOI> {
public:
        casAsyncIOI(const casCtx &ctx, casAsyncIO &ioIn, gdd *pDD=NULL);
	virtual ~casAsyncIOI();

        //
        // place notification of IO completion on the event queue
        //
	caStatus postIOCompletion(caStatus completionStatus, gdd *pDesc=NULL);

	caServer *getCAS();

	//
	// casAsyncIOI must always be a base for casAsyncIO
	// (the constructor assert fails if this isnt the case)
	//
	casAsyncIO * operator -> ()
	{
		return  (casAsyncIO *) this;
	}

	void setServerDelete()
	{
		this->serverDelete = 1u;
	}

	inline void lock();
	inline void unlock();

	gdd *getValuePtr ()
	{
		return this->pDesc;
	}

	void clrValue ()
	{
		if (this->pDesc) {
			gddStatus status;
			status = this->pDesc->Unreference();
			assert(!status);
			this->pDesc = NULL;
		}
	}
private:
        //
        // casEvent virtual call back function
        // (called when IO completion event reaches top of event queue)
        //
        caStatus cbFunc(casEventSys &);

        caHdr const	msg;
        casCoreClient	&client;   
	casChannelI	*pChan; // optional
        gdd		*pDesc; // optional
        caStatus	completionStatus;
	unsigned	inTheEventQueue:1;
	unsigned	posted:1;
	unsigned	ioComplete:1;
	unsigned	serverDelete:1;
};

class casCoreClient;
class casChannel;
class casPVI;

class casChannelI : public tsDLNode<casChannelI>, public casRes {
public:
	casChannelI (const casCtx &ctx, casChannel &chanAdapter);
	~casChannelI();

	void show (unsigned level);

        casCoreClient &getClient() const
	{	
		return this->client;
	}
        const caResId getCID() 
	{
		return this->cid;
	}
	//
	// fetch the unsigned integer server id for this PV
	//
        inline const caResId getSID();

        void postAllModifiedEvents();

	//
	// addMonitor()
	//
        inline void addMonitor(casMonitor &mon);

	//
	// deleteMonitor()
	//
        inline void deleteMonitor(casMonitor &mon);

	//
	// findMonitor
	// (it is reasonable to do a linear search here because
	// sane clients will require only one or two monitors 
	// per channel)
	//
	inline casMonitor *findMonitor(const caResId clientIdIn);

        inline unsigned getIOOPSInProgress() const;

        casPVI &getPVI() const 
	{
		return this->pv;
	}

	inline void installAsyncIO(casAsyncIOI &);
	inline void removeAsyncIO(casAsyncIOI &);

	inline void postEvent (const casEventMask &select, gdd &event);

	casResType resourceType() const 
	{
		return casChanT;
	}

	//
	// casChannelI must always be a base for casPV
	// (the constructor assert fails if this isnt the case)
	//
	casChannel * operator -> ()
	{
		return (casChannel *) this;
	}

	inline void lock();
	inline void unlock();

	inline void clientDestroy();
protected:
        tsDLList<casMonitor>	monitorList;
	tsDLList<casAsyncIOI>	ioInProgList;
        casCoreClient		&client;
        casPVI 			&pv;
        caResId const           cid;    // client id 
	unsigned		clientDestroyPending:1;
};

//
// class hierarchy added here solely because we need two list nodes
//
class casPVListChan : public casChannelI, public tsDLNode<casPVListChan>
{
public:
        inline casPVListChan (const casCtx &ctx, casChannel &chanAdapter);
        inline ~casPVListChan();
};

class caServer;
class caServerI;
class casCtx;
class casChannel;
class casPV;

class casPVI : 
	public stringId,	 // server PV name table installation
	public tsSLNode<casPVI>,  // server PV name table installation 
	public ioBlockedList	// list of clients io blocked on this pv
{
public:

        //
        // The PV name here must be the canonical and unique name
        // for the PV in this system
        //
	casPVI (caServerI &cas, const char * const pName, casPV &pvAdapter);
	~casPVI(); 

        //
        // for use by the server library
        //
        caServerI &getCAS() {return this->cas;}

	static caStatus verifyPVName(gdd &name);

        //
        // CA only does 1D arrays for now (and the new server
        // temporarily does only scalers)
        //
        aitIndex nativeCount() {return 1u; /* scaler */ }

        //
        // only for use by casMonitor
        //
        caStatus registerEvent ();
        void unregisterEvent ();

	//
	// only for use by casAsyncIOI 
	//
	inline void registerIO();
	inline void unregisterIO();

	//
	// how many async IOs are outstanding?
	//
	unsigned ioInProgress() const
	{
		return this->nIOAttached;
	}

	//
	// only for use by casChannelI
	//
	inline void installChannel(casPVListChan &chan);

	//
	// only for use by casChannelI
	//
	inline void removeChannel(casPVListChan &chan);

	//
	// check for none attached and delete self if so
	//
	inline void deleteSignal();

	void show(unsigned level);

	inline void postEvent (const casEventMask &select, gdd &event);

	casPV *intefaceObjectPointer()
	{
		return (casPV *) this;
	}

	//
	// casPVI must always be a base for casPV
	// (the constructor assert fails if this isnt the case)
	//
	casPV * operator -> ()
	{
		return  intefaceObjectPointer();
	}

	caServer *getExtServer();

	//
	// bestDBRType()
	//
	inline caStatus  bestDBRType (unsigned &dbrType);

	inline void lock();
	inline void unlock();
private:
	tsDLList<casPVListChan>	chanList;
	caServerI		&cas;
	unsigned		nMonAttached;
	unsigned		nIOAttached;
};

