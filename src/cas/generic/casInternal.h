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
 * Revision 1.10  1997/01/10 21:17:55  jhill
 * code around gnu g++ inline bug when -O isnt used
 *
 * Revision 1.9  1996/12/06 22:33:49  jhill
 * virtual ~casPVI(), ~casPVListChan(), ~casChannelI()
 *
 * Revision 1.8  1996/11/02 00:54:14  jhill
 * many improvements
 *
 * Revision 1.7  1996/09/16 18:24:02  jhill
 * vxWorks port changes
 *
 * Revision 1.6  1996/09/04 20:21:41  jhill
 * removed operator -> and added member pv
 *
 * Revision 1.5  1996/07/01 19:56:11  jhill
 * one last update prior to first release
 *
 * Revision 1.4  1996/06/26 23:32:17  jhill
 * changed where caProto.h comes from (again)
 *
 * Revision 1.3  1996/06/26 21:18:54  jhill
 * now matches gdd api revisions
 *
 * Revision 1.2  1996/06/20 18:08:35  jhill
 * changed where caProto.h comes from
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */

//
// EPICS
//
#include "tsDLList.h"
#include "resourceLib.h"
#include "caProto.h"

typedef aitUint32 caResId;

class casEventSys;
class casChannelI;

//
// casEvent
//
class casEvent : public tsDLNode<casEvent> {
public:
        virtual ~casEvent();
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
	virtual ~ioBlocked ()=0;
private:
	ioBlockedList	*pList;
	virtual void ioBlockedSignal ();
};

//
// ioBlockedList
//
class ioBlockedList : private tsDLList<ioBlocked> {
public:
        ioBlockedList ();
        ~ioBlockedList ();
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
	inline casMonEvent (casMonEvent &initValue);

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

	inline void operator = (class casMonEvent &monEventIn);
	
	inline void clear();

	void assign (casMonitor &monitor, gdd *pValueIn);
private:
	gdd		*pValue;
	caResId		id;
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

        void post(const casEventMask &select, gdd &value);

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


class caServer;
class casCoreClient;
class casChannelI;
class casCtx;
class caServer;
class casAsyncIO;
class casAsyncReadIO;
class casAsyncWriteIO;
class casAsyncPVExistIO;
class casAsyncPVCreateIO;

class casAsyncIOI : public casEvent, public tsDLNode<casAsyncIOI> {
public:
	casAsyncIOI (casCoreClient &client, casAsyncIO &ioExternal);
	virtual ~casAsyncIOI();

        //
        // place notification of IO completion on the event queue
        //
	caStatus postIOCompletionI();

	inline void lock();
	inline void unlock();

	virtual caStatus cbFuncAsyncIO()=0;
	virtual int readOP();

	void destroyIfReadOP();

	caServer *getCAS() const;

	inline void destroy();

	void reportInvalidAsynchIO(unsigned);

protected:
        casCoreClient	&client;   
	casAsyncIO	&ioExternal;

private:
	unsigned	inTheEventQueue:1;
	unsigned	posted:1;
	unsigned	ioComplete:1;
	unsigned	serverDelete:1;
	unsigned	duplicate:1;
        //
        // casEvent virtual call back function
        // (called when IO completion event reaches top of event queue)
        //
        caStatus cbFunc(casEventSys &);

	inline casAsyncIO * operator -> ();
};

//
// casAsyncRdIOI
//
// (server internal asynchronous read IO class)
//
class casAsyncRdIOI : public casAsyncIOI { 
public:
        casAsyncRdIOI(const casCtx &ctx, casAsyncReadIO &ioIn);
	virtual ~casAsyncRdIOI();

	void destroyIfReadOP();

	caStatus cbFuncAsyncIO();
	casAsyncIO &getAsyncIO();

	caStatus postIOCompletion(caStatus completionStatus,
			gdd &valueRead);
	int readOP();
private:
        caHdr const	msg;
	casChannelI	&chan; 
	gdd		*pDD;
        caStatus	completionStatus;
};

//
// casAsyncWtIOI
//
// (server internal asynchronous write IO class)
//
class casAsyncWtIOI : public casAsyncIOI { 
public:
        casAsyncWtIOI(const casCtx &ctx, casAsyncWriteIO &ioIn);
	virtual ~casAsyncWtIOI();

        //
        // place notification of IO completion on the event queue
        //
	caStatus postIOCompletion(caStatus completionStatus);

	caStatus cbFuncAsyncIO();
	casAsyncIO &getAsyncIO();
private:
        caHdr const	msg;
	casChannelI	&chan; 
        caStatus	completionStatus;
};

union ca_addr;

//
// casOpaqueAddr
//
// store address as an opaque array of bytes so that
// we dont drag the socket (or other IO specific)
// headers into the server tool.
//
//
// get() will assert fail if the init flag has not been
//	set
//
class casOpaqueAddr
{
public:
	//
	// casOpaqueAddr()
	//
	casOpaqueAddr();

	//
	// clear()
	//
	void clear();

	inline int hasBeenInitialized() const;
	inline casOpaqueAddr (const union ca_addr &addr);
	inline void set (const union ca_addr &);
	inline union ca_addr get () const;
private:
	char	opaqueAddr[16u]; // large enough for socket addresses
	char	init;
	
	//
	// simple class that will assert fail if
	// sizeof(opaqueAddr) < sizeof(caAddr)
	//
	class checkSize {
	public:
		checkSize();
	};
	static checkSize sizeChecker;
};

class casDGIntfIO;

//
// casAsyncExIOI 
//
// (server internal asynchronous read IO class)
//
class casAsyncExIOI : public casAsyncIOI { 
public:
        casAsyncExIOI(const casCtx &ctx, casAsyncPVExistIO &ioIn);
	virtual ~casAsyncExIOI();

        //
        // place notification of IO completion on the event queue
        //
	caStatus postIOCompletion(const pvExistReturn retVal);

	caStatus cbFuncAsyncIO();
	casAsyncIO &getAsyncIO();
private:
        caHdr const		msg;
	pvExistReturn 		retVal;
	casDGIntfIO * const 	pOutDGIntfIO;
	const casOpaqueAddr	dgOutAddr;
};

//
// casAsyncPVCIOI 
//
// (server internal asynchronous read IO class)
//
class casAsyncPVCIOI : public casAsyncIOI { 
public:
        casAsyncPVCIOI(const casCtx &ctx, casAsyncPVCreateIO &ioIn);
	virtual ~casAsyncPVCIOI();

        //
        // place notification of IO completion on the event queue
        //
	caStatus postIOCompletion(const pvCreateReturn &retVal);

	caStatus cbFuncAsyncIO();
	casAsyncIO &getAsyncIO();
private:
        caHdr const	msg;
	pvCreateReturn 	retVal;
};

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
	casChannelI (const casCtx &ctx, casChannel &chanAdapter);
	virtual ~casChannelI();

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

        casPVI &getPVI() const 
	{
		return this->pv;
	}

	inline void installAsyncIO(casAsyncIOI &);
	inline void removeAsyncIO(casAsyncIOI &);

	inline void postEvent (const casEventMask &select, gdd &event);

	virtual casResType resourceType() const;

	virtual void show (unsigned level) const;

	virtual void destroy();

	inline void lock() const;
	inline void unlock() const;

	inline void clientDestroy();

	inline casChannel * operator -> () const;

	void clearOutstandingReads();

	//
	// access rights event call back
	//
        caStatus cbFunc(casEventSys &);

	inline void postAccessRightsEvent();
protected:
        tsDLList<casMonitor>	monitorList;
	tsDLList<casAsyncIOI>	ioInProgList;
        casCoreClient		&client;
        casPVI 			&pv;
	casChannel		&chan;
        caResId const           cid;    // client id 
	unsigned		clientDestroyPending:1;
	unsigned		accessRightsEvPending:1;
};

//
// class hierarchy added here solely because we need two list nodes
//
class casPVListChan : public casChannelI, public tsDLNode<casPVListChan>
{
public:
        casPVListChan (const casCtx &ctx, casChannel &chanAdapter);
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
	casPVI (caServer &cas, casPV &pvAdapter);
	virtual ~casPVI(); 

        //
        // for use by the server library
        //
        inline caServerI &getCAS() const;

        //
        // CA only does 1D arrays for now (and the new server
        // temporarily does only scalers)
        //
        inline aitIndex nativeCount();

        //
        // only for use by casMonitor
        //
        caStatus registerEvent ();
        void unregisterEvent ();

	//
	// only for use by casAsyncIOI 
	//
	inline void unregisterIO();

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

	inline void postEvent (const casEventMask &select, gdd &event);

	inline casPV *interfaceObjectPointer() const;

	caServer *getExtServer() const;

	//
	// bestDBRType()
	//
	inline caStatus bestDBRType (unsigned &dbrType);

	inline casPV * operator -> () const;

        virtual casResType resourceType() const;

	virtual void show(unsigned level) const;

	virtual void destroy();

private:
	tsDLList<casPVListChan>	chanList;
	caServerI		&cas;
	casPV			&pv;
	unsigned		nMonAttached;
	unsigned		nIOAttached;
	unsigned		destroyInProgress:1;

	inline void lock() const;
	inline void unlock() const;
};

