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
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

//
// EPICS
//
#include "cxxCompilerDependencies.h"
#include "tsDLList.h"
#include "tsFreeList.h"
#include "resourceLib.h"
#define CA_MINOR_PROTOCOL_REVISION 11
#include "caProto.h"
#include "smartGDDPointer.h"

typedef aitUint32 caResId;

class casChannelI;
class casCoreClient;

//
// casEvent
//
class casEvent : public tsDLNode < casEvent > {
public:
    virtual caStatus cbFunc ( casCoreClient & ) = 0;
    virtual void eventSysDestroyNotify ( casCoreClient & ) = 0;
protected:
    epicsShareFunc virtual ~casEvent();
};

class casChanDelEv : public casEvent {
public:
	casChanDelEv ( caResId idIn ) : id(idIn) {}
	~casChanDelEv ();
private:
	caResId	id;
	caStatus cbFunc ( casCoreClient & ); 
    void eventSysDestroyNotify ( casCoreClient & );
};

enum casResType {casChanT=1, casMonitorT, casPVT};

class casRes : public chronIntIdRes<casRes>
{
public:
	casRes ();
	epicsShareFunc virtual ~casRes();
	virtual casResType resourceType () const = 0;
	virtual void show ( unsigned level ) const = 0;
	//virtual void destroy() = 0;
private:
	casRes ( const casRes & );
	casRes & operator = ( const casRes & );
};

class ioBlockedList;
class epicsMutex;

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
	virtual void ioBlockedSignal ();
};

//
// ioBlockedList
//
class ioBlockedList : private tsDLList<ioBlocked> {
friend class ioBlocked;
public:
	ioBlockedList ();
	epicsShareFunc virtual ~ioBlockedList ();
	void signal ();
	void addItemToIOBLockedList (ioBlocked &item);
	ioBlockedList ( const ioBlockedList & );
	ioBlockedList & operator = ( const ioBlockedList & );
};
 
class casMonitor;

//
// casMonEvent
//
class casMonEvent : public casEvent {
public:
	casMonEvent ();
	casMonEvent (casMonitor &monitor, const smartConstGDDPointer &pNewValue);
	casMonEvent (const casMonEvent &initValue);
	~casMonEvent ();
	smartConstGDDPointer getValue () const;
	void operator = (const class casMonEvent &monEventIn);
	void clear ();
	void assign (casMonitor &monitor, const smartConstGDDPointer &pValueIn);
    void * operator new ( size_t size, tsFreeList < class casMonEvent, 1024 > & );
    epicsPlacementDeleteOperator (( void *, tsFreeList < class casMonEvent, 1024 > & ))
private:
	smartConstGDDPointer pValue;
	caResId id;
    void * operator new ( size_t );
    void operator delete ( void * );
	caStatus cbFunc ( casCoreClient & );
    void eventSysDestroyNotify ( casCoreClient & );
};

class casMonitorCallbackInterface { // X aCC 655
public:
	virtual caStatus casMonitorCallBack ( casMonitor &,
        const smartConstGDDPointer & pValue ) = 0;
};

//
// casMonitor()
//
class casMonitor : 
    public tsDLNode < casMonitor >, public casRes {
public:
	casMonitor ( caResId clientIdIn, casChannelI & chan, 
	    unsigned long nElem, unsigned dbrType,
	    const casEventMask & maskIn, epicsMutex & mutexIn,
        casMonitorCallbackInterface & );
	virtual ~casMonitor();

    void show ( unsigned level ) const;

	caStatus executeEvent ( casMonEvent & );

	void post ( const casEventMask & select, const smartConstGDDPointer & pValue );

	caResId getClientId () const 
	{
		return this->clientId;
	}

	unsigned getType () const 
	{
		return this->dbrType;
	}

	unsigned long getCount () const 
	{
		return this->nElem;
	}
	casChannelI & getChannel () const 
	{	
		return this->ciu;
	}
    void * operator new ( size_t size, 
        tsFreeList < casMonitor, 1024 > & ) 
            epicsThrows ( (std::bad_alloc) );
    epicsPlacementDeleteOperator (( void *, 
        tsFreeList < casMonitor, 1024 > & ))

private:
	casMonEvent overFlowEvent;
	unsigned long const nElem;
	epicsMutex & mutex;
	casChannelI & ciu;
    casMonitorCallbackInterface & callBackIntf;
	const casEventMask mask;
	caResId const clientId;
	unsigned char const dbrType;
	unsigned char nPend;
	bool ovf;
	bool enabled;
	void enable ();
	void disable ();
    casResType resourceType () const;
	void push ( const smartConstGDDPointer & pValue );
    void * operator new ( size_t );
    void operator delete ( void * );
	casMonitor ( const casMonitor & );
	casMonitor & operator = ( const casMonitor & );
};

//
// casMonitor::post()
// (check for NOOP case in line)
//
inline void casMonitor::post (const casEventMask &select, const smartConstGDDPointer &pValue)
{
	casEventMask	result (select&this->mask);
	//
	// NOOP if this event isnt selected
	// or if it is disabled
	//
	if ( result.noEventsSelected() || !this->enabled ) {
		return;
	}

	//
	// else push it on the queue
	//
	this->push (pValue);
}

class caServer;
class casChannelI;
class casCtx;
class caServer;
class casAsyncReadIO;
class casAsyncWriteIO;
class casAsyncPVExistIO;
class casAsyncPVAttachIO;

class casAsyncIOI : public casEvent, public tsDLNode<casAsyncIOI> {
public:
	casAsyncIOI ( const casCtx & ctx );
	epicsShareFunc virtual ~casAsyncIOI ();

	void serverDestroyIfReadOP ();
    void serverDestroy ();

	caServer *getCAS () const;

protected:
	casCoreClient & client;

	//
	// place notification of IO completion on the event queue
	//
	caStatus postIOCompletionI();

private:
	bool inTheEventQueue:1;
	bool posted:1;
	bool ioComplete:1;
	bool serverDelete:1;
	bool duplicate:1;

	//
	// casEvent virtual call back function
	// (called when IO completion event reaches top of event queue)
	//
	epicsShareFunc caStatus cbFunc ( casCoreClient & );

    epicsShareFunc void eventSysDestroyNotify ( casCoreClient & );

    //
    // derived class specic call back
	// (called when IO completion event reaches top of event queue)
    //
    epicsShareFunc virtual caStatus cbFuncAsyncIO() = 0;
    epicsShareFunc virtual void destroy ();

	epicsShareFunc virtual bool readOP() const;

	casAsyncIOI ( const casAsyncIOI & );
	casAsyncIOI & operator = ( const casAsyncIOI & );
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
class casChannelI : public tsDLNode < casChannelI >, 
    public casRes, public casEvent {
public:
	casChannelI ( const casCtx & ctx );
	epicsShareFunc virtual ~casChannelI ();

    void bindToClientI ( class casCoreClient & client, 
        casPVI & pv, caResId cid );

	class casCoreClient & getClient () const
	{	
		return *this->pClient;
	}
	const caResId getCID () 
	{
		return this->cid;
	}
	//
	// fetch the unsigned integer server id for this PV
	//
	const caResId getSID ();

	void installMonitor ( 
        caResId clientId, 
        const unsigned long count, 
        const unsigned type, 
        const casEventMask & );

    bool unistallMonitor ( ca_uint32_t monId );

	casPVI &getPVI () const 
	{
		return *this->pPV;
	}

	void installAsyncIO ( casAsyncIOI & );
	void removeAsyncIO ( casAsyncIOI & );

	void postEvent ( const casEventMask & select, const gdd & event );

	epicsShareFunc virtual casResType resourceType () const;

	void destroyNoClientNotify ();
    void destroyClientNotify ();

	void clearOutstandingReads ();

	void postAccessRightsEvent ();

    const gddEnumStringTable & enumStringTable () const;

    //
    // virtual functions
    //
	epicsShareFunc virtual void setOwner ( const char * const pUserName, 
		const char * const pHostName ) = 0;
	epicsShareFunc virtual bool readAccess () const = 0;
	epicsShareFunc virtual bool writeAccess () const = 0;
	epicsShareFunc virtual bool confirmationRequested () const = 0;
	epicsShareFunc virtual void show ( unsigned level ) const;

protected:
	tsDLList < casMonitor > monitorList;
	tsDLList < casAsyncIOI > ioInProgList;
	class casCoreClient * pClient;
	casPVI * pPV;
	caResId cid;    // client id 
	bool accessRightsEvPending:1;

	epicsShareFunc virtual void destroy ();
	epicsShareFunc caStatus cbFunc ( casCoreClient & ); // access rights event call back
    epicsShareFunc void eventSysDestroyNotify ( casCoreClient & );
	casChannelI ( const casChannelI & );
	casChannelI & operator = ( const casChannelI & );
};

//
// class hierarchy added here solely because we need two list nodes
//
class casPVListChan : public casChannelI, public tsDLNode<casPVListChan>
{
public:
    casPVListChan ( const casCtx &ctx );
    void bindToClient ( casCoreClient & client, 
        casPVI & pv, caResId cidIn );
    epicsShareFunc virtual ~casPVListChan();
private:
	casPVListChan ( const casPVListChan & );
	casPVListChan & operator = ( const casPVListChan & );
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
    epicsShareFunc virtual ~casPVI (); 
    caServerI *getPCAS () const;
    caStatus attachToServer ( caServerI & cas );
    aitIndex nativeCount ();
    caStatus registerEvent ();
    void unregisterEvent ();
    void unregisterIO ();
    void installChannel ( casPVListChan & chan );
    void removeChannel ( casPVListChan & chan );
    void deleteSignal ();
    void postEvent ( const casEventMask & select, const gdd & event );
    caServer * getExtServer () const;
    caStatus bestDBRType ( unsigned & dbrType );
    epicsShareFunc virtual casResType resourceType () const;
    const gddEnumStringTable & enumStringTable () const;
    caStatus updateEnumStringTable ( casCtx & );
    void updateEnumStringTableAsyncCompletion ( const gdd & resp );

    //
    // virtual functions in the public interface class
    //
    epicsShareFunc virtual void show ( unsigned level ) const;
    epicsShareFunc virtual caStatus interestRegister () = 0;
    epicsShareFunc virtual void interestDelete () = 0;
    epicsShareFunc virtual caStatus beginTransaction () = 0;
    epicsShareFunc virtual void endTransaction () = 0;
    epicsShareFunc virtual caStatus read ( const casCtx & ctx, gdd & prototype ) = 0;
    epicsShareFunc virtual caStatus write ( const casCtx & ctx, const gdd & value ) = 0;
    epicsShareFunc virtual casChannel *createChannel (const casCtx & ctx,
        const char * const pUserName, const char * const pHostName ) = 0;
    epicsShareFunc virtual aitEnum bestExternalType () const = 0;
    epicsShareFunc virtual unsigned maxDimension () const = 0; 
    epicsShareFunc virtual aitIndex maxBound ( unsigned dimension ) const = 0;
    epicsShareFunc virtual const char *getName () const = 0;
    epicsShareFunc casPV *apiPointer (); //retuns NULL if casPVI isnt a base of casPV

private:
    tsDLList < casPVListChan > chanList;
    gddEnumStringTable enumStrTbl;
    caServerI * pCAS;
    unsigned nMonAttached;
    unsigned nIOAttached;
    bool destroyInProgress;

    epicsShareFunc virtual void destroy (); // casPVI destructor noop
	casPVI ( const casPVI & );
	casPVI & operator = ( const casPVI & );
};

/* a modified ca header with capacity for large arrays */
struct caHdrLargeArray {
    ca_uint32_t m_postsize;     /* size of message extension */
    ca_uint32_t m_count;        /* operation data count      */
    ca_uint32_t m_cid;          /* channel identifier        */
    ca_uint32_t m_available;    /* protocol stub dependent   */
    ca_uint16_t m_dataType;     /* operation data type       */
    ca_uint16_t m_cmmd;         /* operation to be performed */
};


