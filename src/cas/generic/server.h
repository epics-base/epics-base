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
 */

#ifndef INCLserverh
#define INCLserverh

//
// ANSI C
//
#include <stdio.h>

#if defined(epicsExportSharedSymbols)
#error suspect that libCom, ca, and gdd were not imported
#endif

//
// EPICS
// (some of these are included from casdef.h but
// are included first here so that they are included
// once only before epicsExportSharedSymbols is defined)
//
#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"
#include "epicsAssert.h" // EPICS assert() macros
#include "osiTime.h" // EPICS os independent time
#include "alarm.h" // EPICS alarm severity/condition 
#include "errMdef.h" // EPICS error codes 
#include "gdd.h" // EPICS data descriptors 
#include "resourceLib.h" // EPICS hashing templates

//
// CA
//
#include "caCommonDef.h"
#include "caerr.h"

#if defined(epicsExportSharedSymbols)
#error suspect that libCom, ca, cxxTemplates, and gdd were not imported
#endif

//
// CAS
//
#define epicsExportSharedSymbols
#include "casdef.h" // sets proper def for shareLib.h defines
#include "osiMutexCAS.h" // NOOP on single threaded OS
void casVerifyFunc(const char *pFile, unsigned line, const char *pExp);
void serverToolDebugFunc(const char *pFile, unsigned line, const char *pComment);
#define serverToolDebug(COMMENT) \
{serverToolDebugFunc(__FILE__, __LINE__, COMMENT); } 
#define casVerify(EXP) {if ((EXP)==0) casVerifyFunc(__FILE__, __LINE__, #EXP); } 
caStatus createDBRDD(unsigned dbrType, aitIndex dbrCount, smartGDDPointer &pDescRet);
caStatus copyBetweenDD(gdd &dest, gdd &src);

enum xBlockingStatus {xIsBlocking, xIsntBlocking};
enum casIOState {casOnLine, casOffLine};
typedef unsigned bufSizeT;
static const unsigned bufSizeT_MAX = UINT_MAX;

enum casProcCond {casProcOk, casProcDisconnect};

/*
 * maximum peak log entries for each event block (registartion)
 * (events cached into the last queue entry if over flow occurs)
 * (if this exceeds 256 then the casMonitor::nPend must
 * be assigned a new data type)
 */
#define individualEventEntries 16u

/*
 * maximum average log entries for each event block (registartion)
 * (events cached into the last queue entry if over flow occurs)
 * (if this exceeds 256 then the casMonitor::nPend must
 * be assigned a new data type)
 */
#define averageEventEntries 4u

typedef caResId caEventId;

//
// fwd ref
//
class caServerI;

//
// casEventSys
//
class casEventSys {
friend class casEventPurgeEv;
public:
	casEventSys ();
	virtual ~casEventSys ();

	void show (unsigned level) const;
	casProcCond process ();

	void installMonitor ();
	void removeMonitor ();

	void removeFromEventQueue (casEvent &);
	void addToEventQueue (casEvent &);

	void insertEventQueue (casEvent &insert, casEvent &prevEvent);
	void pushOnToEventQueue (casEvent &event);

	bool full ();

	casMonitor *resIdToMon (const caResId id);

	bool getNDuplicateEvents () const;

	void setDestroyPending ();

	void eventsOn ();

	caStatus eventsOff ();

	virtual caStatus disconnectChan (caResId id) = 0;

private:
	tsDLList<casEvent> eventLogQue;
	osiMutex mutex;
	casEventPurgeEv *pPurgeEvent; // flow control purge complete event
	unsigned numEventBlocks;	// N event blocks installed
	unsigned maxLogEntries; // max log entries
	unsigned char destroyPending;
	unsigned char replaceEvents; // replace last existing event on queue
	unsigned char dontProcess; // flow ctl is on - dont process event queue

	virtual void eventFlush () = 0;
	virtual void eventSignal () = 0;
    virtual casRes *lookupRes (const caResId &idIn, casResType type) = 0;
};

//
// casClientMon
//
class casClientMon : public casMonitor {
public:
	casClientMon(casChannelI &, caResId clientId,
		const unsigned long count, const unsigned type,
		const casEventMask &maskIn, osiMutex &mutexIn);
	virtual ~casClientMon();

	caStatus callBack(gdd &value);

	virtual casResType resourceType() const;

	caResId getId() const
	{
		return this->casRes::getId();
	}

	virtual void destroy();
private:
};

class casCtx {
public:
	inline casCtx();

	//
	// get
	//
	inline const caHdr *getMsg() const;
	inline void *getData() const;
	inline caServerI * getServer() const;
	inline casCoreClient * getClient() const;
	inline casPVI * getPV() const;
	inline casChannelI * getChannel() const;

	//
	// set
	// (assumes incoming message is in network byte order)
	//
	inline void setMsg(const char *pBuf);

	inline void setData(void *p);

	inline void setServer(caServerI *p);

	inline void setClient(casCoreClient *p);

	inline void setPV(casPVI *p);

	inline void setChannel(casChannelI *p);

	void show (unsigned level) const;
	
private:
	caHdr			msg;	// ca message header
	void			*pData; // pointer to data following header
	caServerI               *pCAS;
	casCoreClient		*pClient;
	casChannelI             *pChannel;
	casPVI                  *pPV;
	unsigned		nAsyncIO; // checks for improper use of async io
};

//
// inBufCtx
//
class inBufCtx {
    friend class inBuf;
public:
    enum pushCtxResult {pushCtxNoSpace, pushCtxSuccess};
    inBufCtx (const inBuf &); // success
    inBufCtx (); // failure
    
    pushCtxResult pushResult () const;
    
private:
    pushCtxResult   stat;
    char            *pBuf;
    bufSizeT        bufSize;
    bufSizeT        bytesInBuffer;
    bufSizeT        nextReadIndex;
};

//
// inBuf 
//
class inBuf {
    friend class inBufCtx;
public:

    enum fillCondition {casFillNone,  casFillProgress,  
        casFillDisconnect};

    // this is a hack for a Solaris IP kernel feature
    enum fillParameter {fpNone, fpUseBroadcastInterface};
    
    inBuf (bufSizeT bufSizeIn, bufSizeT ioMinSizeIn);
    virtual ~inBuf ();
    
    bufSizeT bytesPresent () const;
    
    bufSizeT bytesAvailable () const;
    
    bool full () const;
    
    //
    // fill the input buffer with any incoming messages
    //
    fillCondition fill (enum fillParameter parm=fpNone);
    
    void show (unsigned level) const;

protected:

    void clear ();
    
    char *msgPtr () const;
    
    void removeMsg (bufSizeT nBytes);

    //
    // This is used to create recursive protocol stacks. A subsegment
    // of the buffer of max size "maxSize" is assigned to the next
    // layer down in the protocol stack by pushCtx () until popCtx ()
    // is called. The roiutine popCtx () returns the actual number
    // of bytes used by the next layer down.
    //
    // pushCtx() returns an outBufCtx to be restored by popCtx()
    //
    const inBufCtx pushCtx (bufSizeT headerSize, bufSizeT bodySize);
	bufSizeT popCtx (const inBufCtx &); // returns actual size

private:
    osiMutex    mutex;
    char        *pBuf;
    bufSizeT    bufSize;
    bufSizeT    bytesInBuffer;
    bufSizeT    nextReadIndex;
    bufSizeT    ioMinSize;
    unsigned    ctxRecursCount;

    virtual unsigned getDebugLevel () const = 0;
    virtual bufSizeT incommingBytesPresent () const = 0;
    virtual fillCondition xRecv (char *pBuf, bufSizeT nBytesToRecv, 
        enum inBuf::fillParameter parm, bufSizeT &nByesRecv) = 0;
    virtual void clientHostName (char *pBuf, unsigned bufSize) const = 0;
};

//
// outBufCtx
//
class outBufCtx {
    friend class outBuf;
public:
    enum pushCtxResult {pushCtxNoSpace, pushCtxSuccess};
    outBufCtx (const outBuf &); // success
    outBufCtx (); // failure

    pushCtxResult pushResult () const;

private:
    pushCtxResult   stat;
	char            *pBuf;
	bufSizeT        bufSize;
	bufSizeT        stack;
};

//
// outBuf
//
class outBuf {
    friend class outBufCtx;
public:

    enum flushCondition {flushNone, flushProgress, flushDisconnect};

	outBuf (bufSizeT bufSizeIn);
	virtual ~outBuf ()=0;

	bufSizeT bytesPresent() const;

	//
	// flush output queue
	// (returns the number of bytes sent)
    //
	flushCondition flush (bufSizeT spaceRequired=bufSizeT_MAX);

	void show (unsigned level) const;

protected:

	//
	// allocate message buffer space
	// (leaves message buffer locked)
	//
	caStatus allocMsg (bufSizeT extsize, caHdr **ppMsg);

	//
	// allocate message buffer space
	// (leaves message buffer locked)
	//
	caStatus allocRawMsg (bufSizeT size, void **ppMsg);

    //
    // This is used to create recursive protocol stacks. A subsegment
    // of the buffer of max size "maxSize" is assigned to the next
    // layer down in the protocol stack by pushCtx () until popCtx ()
    // is called. The roiutine popCtx () returns the actual number
    // of bytes used by the next layer down.
    //
    // pushCtx() returns an outBufCtx to be restored by popCtx()
    //
    const outBufCtx pushCtx (bufSizeT headerSize, bufSizeT maxBodySize, void *&pHeader);
	bufSizeT popCtx (const outBufCtx &); // returns actual size

	//
	// commits message allocated with allocMsg()
	//
	void commitMsg ();

	//
	// commits message allocated with allocRawMsg()
	//
	void commitRawMsg (bufSizeT size);

	void clear ();

private:
    osiMutex    mutex;
	char        *pBuf;
	bufSizeT    bufSize;
	bufSizeT    stack;
    unsigned    ctxRecursCount;

	virtual unsigned getDebugLevel() const = 0;
	virtual void sendBlockSignal() = 0;

	//
	// io dependent
	//
	virtual flushCondition xSend (char *pBuf, bufSizeT nBytesAvailableToSend, 
		bufSizeT nBytesNeedToBeSent, bufSizeT &nBytesSent) = 0;
	virtual void clientHostName (char *pBuf, unsigned bufSize) const = 0;
};

//
// casCoreClient
// (this will eventually support direct communication
// between the client lib and the server lib)
//
class casCoreClient : public osiMutex, public ioBlocked, 
	public casEventSys {

	//
	// allows casAsyncIOI constructor to check for asynch IO duplicates
	//
	friend casAsyncIOI::casAsyncIOI(casCoreClient &clientIn);

public:
	casCoreClient(caServerI &serverInternal); 
	virtual ~casCoreClient();
	virtual void destroy();
	virtual caStatus disconnectChan(caResId id);
	virtual void show (unsigned level) const;
	virtual void installChannel (casChannelI &);
	virtual void removeChannel (casChannelI &);

	void installAsyncIO(casAsyncIOI &ioIn);

	void removeAsyncIO(casAsyncIOI &ioIn);

	casRes *lookupRes(const caResId &idIn, casResType type);

	caServerI &getCAS() const;

	virtual caStatus monitorResponse (casChannelI &chan, const caHdr &msg, 
		const gdd *pDesc, const caStatus status);

	virtual caStatus accessRightsResponse(casChannelI *);

	//
	// one virtual function for each CA request type that has
	// asynchronous completion
	//
	virtual caStatus asyncSearchResponse(
		const caNetAddr &outAddr, 
		const caHdr &, const pvExistReturn &);
	virtual caStatus createChanResponse(
		const caHdr &, const pvAttachReturn &);
	virtual caStatus readResponse(
		casChannelI *, const caHdr &, const gdd &, const caStatus); 
	virtual caStatus readNotifyResponse(
		casChannelI *, const caHdr &, const gdd *, const caStatus);
	virtual caStatus writeResponse (const caHdr &, const caStatus);
	virtual caStatus writeNotifyResponse (const caHdr &, const caStatus);

	//
	// used only with DG clients 
	// 
	virtual caNetAddr fetchLastRecvAddr () const;

protected:
	casCtx			ctx;
	unsigned char		asyncIOFlag;

private:
	tsDLList<casAsyncIOI>   ioInProgList;
};

//
// casClient
//
class casClient : public casCoreClient, public inBuf, public outBuf {
public:
    typedef caStatus (casClient::*pCASMsgHandler) ();

	casClient (caServerI &, bufSizeT ioMinSizeIn);
	virtual ~casClient ();

	void show (unsigned level) const;

	//
	// send error response to a message
	//
	caStatus sendErr (const caHdr *, const int reportedStatus,
			const char *pFormat, ...);

	unsigned getMinorVersion() const {return this->minor_version_number;}


	//
	// find the channel associated with a resource id
	//
	casChannelI *resIdToChannel (const caResId &id);

	virtual void clientHostName (char *pBuf, unsigned bufSize) const = 0;

protected:
	unsigned	minor_version_number;
	osiTime		lastSendTS;
	osiTime		lastRecvTS;

	caStatus sendErrWithEpicsStatus(const caHdr *pMsg,
			caStatus epicsStatus, caStatus clientStatus);

#	define logBadId(MP, DP, CACSTAT, RESID) \
	this->logBadIdWithFileAndLineno(MP, DP, CACSTAT, __FILE__, __LINE__, RESID)
	caStatus logBadIdWithFileAndLineno(const caHdr *mp,
			const void *dp, const int cacStat, const char *pFileName, 
			const unsigned lineno, const unsigned resId);

	caStatus processMsg();

	//
	// dump message to stderr
	//
	void dumpMsg(const caHdr *mp, const void *dp);

private:

	//
	// one function for each CA request type
	//
	virtual caStatus uknownMessageAction () = 0;
	caStatus ignoreMsgAction ();
	caStatus noopAction ();
	virtual caStatus eventAddAction ();
	virtual caStatus eventCancelAction ();
	virtual caStatus readAction ();
	virtual caStatus readNotifyAction ();
	virtual caStatus writeAction ();
	virtual caStatus searchAction ();
	virtual caStatus eventsOffAction ();
	virtual caStatus eventsOnAction ();
	virtual caStatus readSyncAction ();
	virtual caStatus clearChannelAction ();
	virtual caStatus claimChannelAction ();
	virtual caStatus writeNotifyAction ();
	virtual caStatus clientNameAction ();
	virtual caStatus hostNameAction ();
	virtual caStatus echoAction ();

	//
	// obtain the user name and host from the derived class
	//
	virtual const char *hostName() const;
	virtual const char *userName() const;

	//
	// static members
	//
	static void loadProtoJumpTable();
	static pCASMsgHandler msgHandlers[CA_PROTO_LAST_CMMD+1u];
	static int msgHandlersInit;
};

//
// casStrmClient 
//
class casStrmClient : public casClient,
	public tsDLNode<casStrmClient> {
public:
	casStrmClient (caServerI &cas);
	virtual ~casStrmClient();

	void show (unsigned level) const;

	//
	// installChannel()
	//
	void installChannel(casChannelI &chan);

	//
	// removeChannel()
	//
	void removeChannel(casChannelI &chan);

	//
	// one function for each CA request type that has
	// asynchronous completion
	//
	virtual caStatus createChanResponse(const caHdr &, const pvAttachReturn &);
	caStatus readResponse(casChannelI *pChan, const caHdr &msg,
			const gdd &desc, const caStatus status);
	caStatus readNotifyResponse(casChannelI *pChan, const caHdr &msg,
			const gdd *pDesc, const caStatus status);
	caStatus writeResponse(const caHdr &msg,
			const caStatus status);
	caStatus writeNotifyResponse(const caHdr &msg, const caStatus status);
	caStatus monitorResponse(casChannelI &chan, const caHdr &msg, 
		const gdd *pDesc, const caStatus status);

	caStatus noReadAccessEvent(casClientMon *);

	//
	// obtain the user name and host 
	//
	const char *hostName () const;
	const char *userName () const;

	caStatus disconnectChan (caResId id);

	unsigned getDebugLevel() const;

    virtual void clientHostName (char *pBuf, unsigned bufSize) const = 0;

protected:

private:
	tsDLList<casChannelI>	chanList;
	char                    *pUserName;
	char                    *pHostName;

	//
	// createChannel()
	//
	caStatus createChannel (const char *pName);

	//
	// verify read/write requests
	//
	caStatus verifyRequest (casChannelI *&pChan);

	//
	// one function for each CA request type
	//
    caStatus uknownMessageAction ();
	caStatus eventAddAction ();
	caStatus eventCancelAction ();
	caStatus readAction ();
	caStatus readNotifyAction ();
	caStatus writeAction ();
	caStatus eventsOffAction ();
	caStatus eventsOnAction ();
	caStatus readSyncAction ();
	caStatus clearChannelAction ();
	caStatus claimChannelAction ();
	caStatus writeNotifyAction ();
	caStatus clientNameAction ();
	caStatus hostNameAction ();

	//
	// accessRightsResponse()
	//
	caStatus accessRightsResponse (casChannelI *pciu);
	//

	// these prepare the gdd based on what is in the ca hdr
	//
	caStatus read (smartGDDPointer &pDesc);
	caStatus write ();

	//
	// channelCreateFailed()
	//
	caStatus channelCreateFailed (const caHdr *mp, caStatus createStatus);

	caStatus writeArrayData();
	caStatus writeScalarData();
	caStatus writeString();

	//
	// io independent send/recv
	//
    outBuf::flushCondition xSend (char *pBuf, bufSizeT nBytesAvailableToSend,
			bufSizeT nBytesNeedToBeSent, bufSizeT &nBytesSent);
    inBuf::fillCondition xRecv (char *pBuf, bufSizeT nBytesToRecv,
			fillParameter parm, bufSizeT &nByesRecv);

	virtual xBlockingStatus blockingState() const = 0;

	virtual outBuf::flushCondition osdSend (const char *pBuf, bufSizeT nBytesReq,
			bufSizeT &nBytesActual) = 0;
	virtual inBuf::fillCondition osdRecv (char *pBuf, bufSizeT nBytesReq,
			bufSizeT &nBytesActual) = 0;

	caStatus readNotifyResponseECA_XXX (casChannelI *pChan, 
		const caHdr &msg, const gdd *pDesc, const caStatus ecaStatus);
	caStatus writeNotifyResponseECA_XXX(const caHdr &msg,
			const caStatus status);
};

class casDGIntfIO;


//
// casDGClient 
//
class casDGClient : public casClient {
public:
	casDGClient (caServerI &serverIn);
	virtual ~casDGClient();

	void show (unsigned level) const;

	//
	// only for use with DG io
	//
	void sendBeacon ();
    virtual void sendBeaconIO (char &msg, unsigned length, aitUint16 &m_port) = 0;

	void destroy();

	unsigned getDebugLevel() const;

    void clientHostName (char *pBuf, unsigned bufSize) const;

	caNetAddr fetchLastRecvAddr () const;

    virtual caNetAddr serverAddress () const = 0;


protected:

	casProcCond eventSysProcess()
	{
		return this->casEventSys::process();
	}

    caStatus processDG ();

private:
    caNetAddr lastRecvAddr;

	//
	// one function for each CA request type
	//
	caStatus searchAction ();
    caStatus uknownMessageAction ();

	//
	// searchFailResponse()
	//
	caStatus searchFailResponse (const caHdr *pMsg);

	caStatus searchResponse (const caHdr &, const pvExistReturn &);

	caStatus asyncSearchResponse (const caNetAddr &outAddr, 
		const caHdr &msg, const pvExistReturn &);

	//
	// IO depen
	//
	outBuf::flushCondition xSend (char *pBufIn, bufSizeT nBytesAvailableToSend, 
		bufSizeT nBytesNeedToBeSent, bufSizeT &nBytesSent);
	inBuf::fillCondition xRecv (char *pBufIn, bufSizeT nBytesToRecv, 
        fillParameter parm, bufSizeT &nByesRecv);

	virtual outBuf::flushCondition osdSend (const char *pBuf, bufSizeT nBytesReq,
			const caNetAddr &addr) = 0;
	virtual inBuf::fillCondition osdRecv (char *pBuf, bufSizeT nBytesReq,
			fillParameter parm, bufSizeT &nBytesActual, caNetAddr &addr) = 0;

    //
    // cadg
    //
    struct cadg {
        caNetAddr cadg_addr; // invalid address indicates pad
        bufSizeT cadg_nBytes;
    };

};

//
// casEventMaskEntry
//
class casEventMaskEntry : public tsSLNode<casEventMaskEntry>,
	public casEventMask, public stringId {
public:
	casEventMaskEntry (casEventRegistry &regIn,
	    casEventMask maskIn, const char *pName);
	virtual ~casEventMaskEntry();
	void show (unsigned level) const;

	virtual void destroy();
private:
	casEventRegistry        &reg;
};

static const unsigned casEventRegistryHashTableSize = 256u;

//
// casEventRegistry
//
class casEventRegistry : private resTable <casEventMaskEntry, stringId> {
    friend class casEventMaskEntry;
public:
    
    casEventRegistry () :  
        resTable<casEventMaskEntry, stringId> (casEventRegistryHashTableSize), 
        allocator(0) {}
    
    virtual ~casEventRegistry();
    
    casEventMask registerEvent (const char *pName);
    
    void show (unsigned level) const;
    
private:
    unsigned allocator;
    
    casEventMask maskAllocator();
};

#include "casIOD.h" // IO dependent
#include "casOSD.h" // OS dependent

class casClientMon;

//
// caServerI
// 
class caServerI : 
	public osiMutex, // osiMutex must be first because it is used
			// by ioBlockedList and casEventRegistry
	public caServerOS, 
	public caServerIO, 
	public ioBlockedList, 
	private chronIntIdResTable<casRes>,
	public casEventRegistry {
public:
	caServerI (caServer &tool, unsigned pvCountEstimate);
	~caServerI ();

	//
	// find the channel associated with a resource id
	//
	casChannelI *resIdToChannel(const caResId &id);

	//
	// find the PV associated with a resource id
	//
	casPVI *resIdToPV(const caResId &id);

	//
	// find the client monitor associated with a resource id
	//
	casClientMon *resIdToClientMon(const caResId &idIn);

	void installClient (casStrmClient *pClient);

	void removeClient (casStrmClient *pClient);

	//
	// is there space for a new channel
	//
	bool roomForNewChannel() const;

	unsigned getDebugLevel() const { return debugLevel; }
	inline void setDebugLevel (unsigned debugLevelIn);

	void show(unsigned level) const;

	casRes *lookupRes (const caResId &idIn, casResType type);

	caServer *getAdapter ();

	void installItem (casRes &res);

	casRes *removeItem (casRes &res);

	//
	// call virtual function in the interface class
	//
	caServer * operator -> ();

	void connectCB (casIntfOS &);

    //
	// common event masks 
	// (what is currently used by the CA clients)
	//
	casEventMask valueEventMask() const; // DBE_VALUE registerEvent("value")
	casEventMask logEventMask() const; 	// DBE_LOG registerEvent("log") 
	casEventMask alarmEventMask() const; // DBE_ALARM registerEvent("alarm") 

    unsigned readEventsProcessedCounter (void) const;
    void incrEventsProcessedCounter (void);
    void clearEventsProcessedCounter (void);

    unsigned readEventsPostedCounter (void) const;
    void incrEventsPostedCounter (void);
    void clearEventsPostedCounter (void);

private:
	void advanceBeaconPeriod();

	tsDLList<casStrmClient> clientList;
    tsDLList<casIntfOS>     intfList;
	double                  maxBeaconInterval;
	double                  beaconPeriod;
	caServer                &adapter;
	unsigned                debugLevel;
    unsigned                nEventsProcessed; 
    unsigned                nEventsPosted; 

    //
    // predefined event types
    //
    casEventMask            valueEvent; // DBE_VALUE registerEvent("value")
	casEventMask            logEvent; 	// DBE_LOG registerEvent("log") 
	casEventMask            alarmEvent; // DBE_ALARM registerEvent("alarm")

	double getBeaconPeriod() const;

	//
	// send beacon and advance beacon timer
	//
	void sendBeacon();

	caStatus attachInterface (const caNetAddr &addr, bool autoBeaconAddr,
			bool addConfigAddr);
};

#define CAServerConnectPendQueueSize 10

/*
 * If there is insufficent space to allocate an 
 * asynch IO in progress block then we will end up
 * with pIoInProgress nill and activeAsyncIO true.
 * This protects the app from simultaneous multiple
 * invocation of async IO on the same PV.
 */
#define maxIOInProg 50 

/*
 * when this event reaches the top of the queue we
 * know that all duplicate events have been purged
 * and that now no events should not be sent to the
 * client until it exits flow control mode
 */
class casEventPurgeEv : public casEvent {
public:
private:
	caStatus cbFunc(casEventSys &);
};


/*
 * this really should be in another header file
 */
extern "C" {
void		ca_printf (const char *pFormat, ...);
}

#endif /*INCLserverh*/


