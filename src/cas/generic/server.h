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

enum xRecvStatus {xRecvOK, xRecvDisconnect};
enum xSendStatus {xSendOK, xSendDisconnect};
enum xBlockingStatus {xIsBlocking, xIsntBlocking};
enum casIOState {casOnLine, casOffLine};
typedef unsigned bufSizeT;

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
	inline casEventSys (casCoreClient &coreClientIn);

	inline caStatus init();

	virtual ~casEventSys();

	virtual void destroy()=0;

	void show(unsigned level) const;
	casProcCond process();

	void installMonitor();
	void removeMonitor();

	inline void removeFromEventQueue(casEvent &);
	inline void addToEventQueue(casEvent &);

	inline void insertEventQueue(casEvent &insert, casEvent &prevEvent);
	inline void pushOnToEventQueue(casEvent &event);

	inline aitBool full();

	inline casMonitor *resIdToMon(const caResId id);

	inline casCoreClient &getCoreClient();


	inline aitBool getNDuplicateEvents () const;

	inline void setDestroyPending();

	void eventsOn();

	caStatus eventsOff();

private:
	tsDLList<casEvent> eventLogQue;
	osiMutex mutex;
	casCoreClient &coreClient;
	casEventPurgeEv *pPurgeEvent; // flow control purge complete event
	unsigned numEventBlocks;	// N event blocks installed
	unsigned maxLogEntries; // max log entries
	unsigned char destroyPending;
	unsigned char replaceEvents; // replace last existing event on queue
	unsigned char dontProcess; // flow ctl is on - dont process event queue
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

enum casFillCondition{
	casFillNone, 
	casFillPartial, 
	casFillFull,
	casFillDisconnect};
//
// inBuf 
//
class inBuf {
public:
	//
	// this needs to be here (and not in dgInBufIL.h) if we
	// are to avoid undefined symbols under gcc 2.7.x without -O
	//
	inline inBuf(osiMutex &mutexIn, unsigned bufSizeIn);
	inline caStatus init(); //constructor does not return status
	virtual ~inBuf();

	inline bufSizeT bytesPresent() const;

	inline bufSizeT bytesAvailable() const ;

	inline aitBool full() const;

	inline void clear();

	inline char *msgPtr() const;

	inline void removeMsg(unsigned nBytes);

	//
	// fill the input buffer with any incoming messages
	//
	casFillCondition fill ();

	void show (unsigned level) const;

	virtual unsigned getDebugLevel() const = 0;
	virtual bufSizeT incommingBytesPresent() const = 0;
    virtual xRecvStatus xRecv (char *pBuf, bufSizeT nBytesToRecv,
		bufSizeT &nByesRecv) = 0;
    virtual void clientHostName (char *pBuf, 
		unsigned bufSize) const = 0;

private:
	osiMutex	&mutex;
        char            *pBuf;
        bufSizeT        bufSize;
        bufSizeT        bytesInBuffer;
        bufSizeT        nextReadIndex;
};

//
// dgInBuf 
//
class dgInBuf : public inBuf {
public:
	dgInBuf (osiMutex &mutexIn, unsigned bufSizeIn); 
	virtual ~dgInBuf();

	inline void clear();

	int hasAddress() const;

	caNetAddr getSender() const;

	xRecvStatus xRecv (char *pBufIn, bufSizeT nBytesToRecv,
		bufSizeT &nByesRecv);

	virtual xRecvStatus xDGRecv (char *pBuf, bufSizeT nBytesToRecv,
		bufSizeT &nByesRecv, caNetAddr &sender) = 0;

private:
	caNetAddr	from;
};

//
// outBuf
//
enum casFlushCondition{
	casFlushNone,
	casFlushPartial, 
	casFlushCompleted,
	casFlushDisconnect};
enum casFlushRequest{
	casFlushAll,
	casFlushSpecified};

class outBuf {
public:
	outBuf (osiMutex &, unsigned bufSizeIn);
	caStatus init(); //constructor does not return status
	virtual ~outBuf()=0;

	inline bufSizeT bytesPresent() const;

	inline bufSizeT bytesFree() const;

	//
	// flush output queue
	// (returns the number of bytes sent)
	//
	casFlushCondition flush(casFlushRequest req = casFlushAll, 
		bufSizeT spaceRequired=0u);

	//
	// allocate message buffer space
	// (leaves message buffer locked)
	//
	caStatus allocMsg (unsigned extsize, caHdr **ppMsg, int lockRequired=1);

	//
	// commits message allocated with allocMsg()
	//
	void commitMsg ();

	void show(unsigned level) const;

	virtual unsigned getDebugLevel() const = 0;
	virtual void sendBlockSignal();

	//
	// io dependent
	//
	virtual xSendStatus xSend (char *pBuf, bufSizeT nBytesAvailableToSend, 
		bufSizeT nBytesNeedToBeSent, bufSizeT &nBytesSent) = 0;
	virtual void clientHostName (char *pBuf, unsigned bufSize) const = 0;

	inline void clear();

private:
	osiMutex	&mutex;
	char            *pBuf;
	const bufSizeT  bufSize;
	bufSizeT        stack;
};

//
// dgOutBuf
//
class dgOutBuf : public outBuf {
public:
	dgOutBuf (osiMutex &mutexIn, unsigned bufSizeIn);

	virtual ~dgOutBuf();

	caNetAddr getRecipient();

	void setRecipient(const caNetAddr &addr);

	void clear();

	xSendStatus xSend (char *pBufIn, bufSizeT nBytesAvailableToSend, 
		bufSizeT nBytesNeedToBeSent, bufSizeT &nBytesSent);

	virtual xSendStatus xDGSend (char *pBuf, bufSizeT nBytesNeedToBeSent, 
		bufSizeT &nBytesSent, const caNetAddr &recipient) = 0;
private:
	caNetAddr	to;	
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
	friend casAsyncIOI::casAsyncIOI(casCoreClient &clientIn, casAsyncIO &ioExternalIn);

public:
	casCoreClient(caServerI &serverInternal); 
	caStatus init();

	virtual ~casCoreClient();
	virtual void destroy();
	virtual caStatus disconnectChan(caResId id);
	virtual void eventSignal() = 0;
	virtual void eventFlush() = 0;
	virtual caStatus start () = 0;
	virtual void show (unsigned level) const;
	virtual void installChannel (casChannelI &);
	virtual void removeChannel (casChannelI &);

	inline void installAsyncIO(casAsyncIOI &ioIn);

	inline void removeAsyncIO(casAsyncIOI &ioIn);

	inline casRes *lookupRes(const caResId &idIn, casResType type);

	inline caServerI &getCAS() const;

	virtual caStatus monitorResponse(casChannelI &chan, const caHdr &msg, 
		const gdd *pDesc, const caStatus status);

	virtual caStatus accessRightsResponse(casChannelI *);

	//
	// one virtual function for each CA request type that has
	// asynchronous completion
	//
	virtual caStatus asyncSearchResponse(
		casDGIntfIO &outMsgIO, const caNetAddr &outAddr, 
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
	// The following are only used with async IO for
	// DG clients 
	// 
	virtual caNetAddr fetchRespAddr();
	virtual casDGIntfIO* fetchOutIntf();
protected:
	casCtx			ctx;
	unsigned char		asyncIOFlag;

private:
	tsDLList<casAsyncIOI>   ioInProgList;
};

//
// casClient
//
class casClient;
typedef caStatus (casClient::*pCASMsgHandler) ();
class casClient : public casCoreClient {
public:
	casClient (caServerI &, inBuf &, outBuf &);
	caStatus init(); //constructor does not return status
	virtual ~casClient ();

	void show(unsigned level) const;

	//
	// send error response to a message
	//
	caStatus sendErr(const caHdr *, const int reportedStatus,
			const char *pFormat, ...);

	unsigned getMinorVersion() const {return this->minor_version_number;}


	//
	// find the channel associated with a resource id
	//
	casChannelI *resIdToChannel(const caResId &id);

	virtual void clientHostName (char *pBuf, 
		unsigned bufSize) const = 0;

protected:
	unsigned	minor_version_number;
	osiTime		lastSendTS;
	osiTime		lastRecvTS;

	caStatus processMsg();


	//
	//
	//
	caStatus sendErrWithEpicsStatus(const caHdr *pMsg,
			caStatus epicsStatus, caStatus clientStatus);

	//
	// logBadIdWithFileAndLineno()
	//
#	define logBadId(MP, DP, CACSTAT, RESID) \
	this->logBadIdWithFileAndLineno(MP, DP, CACSTAT, __FILE__, __LINE__, RESID)
	caStatus logBadIdWithFileAndLineno(const caHdr *mp,
			const void *dp, const int cacStat, const char *pFileName, 
			const unsigned lineno, const unsigned resId);

private:
	inBuf &inBufRef;
	outBuf &outBufRef;

	//
	// dump message to stderr
	//
	void dumpMsg(const caHdr *mp, const void *dp);

	//
	// one function for each CA request type
	//
	caStatus uknownMessageAction ();
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
class casStrmClient : public inBuf, public outBuf, public casClient,
	public tsDLNode<casStrmClient> {
public:
	casStrmClient (caServerI &cas);
	caStatus init(); //constructor does not return status
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

	//
	//
	//
	caStatus noReadAccessEvent(casClientMon *);

	//
	// obtain the user name and host 
	//
	const char *hostName () const;
	const char *userName () const;

	caStatus disconnectChan (caResId id);

	unsigned getDebugLevel() const;
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
	xSendStatus xSend (char *pBuf, bufSizeT nBytesAvailableToSend,
			bufSizeT nBytesNeedToBeSent, bufSizeT &nBytesSent);
	xRecvStatus xRecv (char *pBuf, bufSizeT nBytesToRecv,
			bufSizeT &nByesRecv);
	virtual xBlockingStatus blockingState() const = 0;
	virtual xSendStatus osdSend (const char *pBuf, bufSizeT nBytesReq,
			bufSizeT &nBytesActual) = 0;
	virtual xRecvStatus osdRecv (char *pBuf, bufSizeT nBytesReq,
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
class casDGClient : public dgInBuf, public dgOutBuf, public casClient {
public:
	casDGClient (caServerI &serverIn);
	virtual ~casDGClient();

	caStatus init(); // constructor does not return status

	void show (unsigned level) const;

	//
	// only for use with DG io
	//
	static void sendBeacon(casDGIntfIO &io);

	void destroy();

	void processDG(casDGIntfIO &inMsgIO, casDGIntfIO &outMsgIO);

	unsigned getDebugLevel() const;

    virtual void clientHostName (char *pBuf, 
		unsigned bufSize) const = 0;

protected:

	casProcCond eventSysProcess()
	{
		return this->casEventSys::process();
	}

private:
	casDGIntfIO *pOutMsgIO;
	casDGIntfIO *pInMsgIO;

	void ioBlockedSignal(); // dummy

	//
	// one function for each CA request type
	//
	caStatus searchAction ();

	//
	// searchFailResponse()
	//
	caStatus searchFailResponse(const caHdr *pMsg);

	caStatus searchResponse(const caHdr &, const pvExistReturn &);

	caStatus asyncSearchResponse(
		casDGIntfIO &outMsgIO, const caNetAddr &outAddr, 
		const caHdr &msg, const pvExistReturn &);
	caNetAddr fetchRespAddr();
	casDGIntfIO* fetchOutIntf();

	//
	// IO depen
	//
	xRecvStatus xDGRecv (char *pBuf, bufSizeT nBytesToRecv,
			bufSizeT &nByesRecv, caNetAddr &sender);
	xSendStatus xDGSend (char *pBuf, bufSizeT nBytesNeedToBeSent, 
			bufSizeT &nBytesSent, const caNetAddr &recipient);
	bufSizeT incommingBytesPresent() const;
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
    
    casEventRegistry (osiMutex &mutexIn) :  
        resTable<casEventMaskEntry, stringId> (casEventRegistryHashTableSize), 
        mutex(mutexIn), allocator(0) {}
    
    virtual ~casEventRegistry();
    
    casEventMask registerEvent (const char *pName);
    
    void show (unsigned level) const;
    
private:
    osiMutex &mutex;
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
	inline casChannelI *resIdToChannel(const caResId &id);

	//
	// find the PV associated with a resource id
	//
	casPVI *resIdToPV(const caResId &id);

	//
	// find the client monitor associated with a resource id
	//
	casClientMon *resIdToClientMon(const caResId &idIn);

	casDGClient &castClient() {return this->dgClient;}

	void installClient(casStrmClient *pClient);

	void removeClient(casStrmClient *pClient);

	//
	// is there space for a new channel
	//
	aitBool roomForNewChannel() const;

	//
	// send beacon and advance beacon timer
	//
	void sendBeacon();

	unsigned getDebugLevel() const { return debugLevel; }
	inline void setDebugLevel(unsigned debugLevelIn);

	double getBeaconPeriod() const { return this->beaconPeriod; }

	void show(unsigned level) const;

	inline casRes *lookupRes(const caResId &idIn, casResType type);

	inline caServer *getAdapter();

	inline void installItem (casRes &res);

	inline casRes *removeItem(casRes &res);

	//
	// call virtual function in the interface class
	//
	inline caServer * operator -> ();

	void connectCB(casIntfOS &);

	inline aitBool ready();

	caStatus addAddr(const caNetAddr &addr, int autoBeaconAddr,
			int addConfigAddr);
private:
	void advanceBeaconPeriod();

	casDGOS                 dgClient;
	//casCtx                ctx;
	tsDLList<casStrmClient> clientList;
	tsDLList<casIntfOS>     intfList;
	double                  maxBeaconInterval;
	double                  beaconPeriod;
	caServer                &adapter;
	unsigned                debugLevel;
	unsigned char           haveBeenInitialized;
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


