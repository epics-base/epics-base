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

#ifndef INCLserverh
#define INCLserverh

//
// ANSI C
//
#include <stdio.h>

#if defined(epicsExportSharedSymbols)
#   error suspect that libCom, ca, and gdd were not imported
#endif

//
// EPICS
// (some of these are included from casdef.h but
// are included first here so that they are included
// once only before epicsExportSharedSymbols is defined)
//
#include "gdd.h" // EPICS data descriptors 
#undef epicsAssertAuthor
#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"
#include "epicsAssert.h" // EPICS assert() macros
#include "epicsTime.h" // EPICS os independent time
#include "alarm.h" // EPICS alarm severity/condition 
#include "errMdef.h" // EPICS error codes 
#include "resourceLib.h" // EPICS hashing templates
#include "errlog.h" // EPICS error logging interface
#include "epicsSingleton.h"

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
#include "epicsMutex.h" 
void casVerifyFunc(const char *pFile, unsigned line, const char *pExp);
void serverToolDebugFunc(const char *pFile, unsigned line, const char *pComment);
#define serverToolDebug(COMMENT) \
{serverToolDebugFunc(__FILE__, __LINE__, COMMENT); } 
#define casVerify(EXP) {if ((EXP)==0) casVerifyFunc(__FILE__, __LINE__, #EXP); } 
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
	epicsMutex mutex;
	casEventPurgeEv *pPurgeEvent; // flow control purge complete event
	unsigned numEventBlocks;	// N event blocks installed
	unsigned maxLogEntries; // max log entries
	unsigned char destroyPending;
	unsigned char replaceEvents; // replace last existing event on queue
	unsigned char dontProcess; // flow ctl is on - dont process event queue

	virtual void eventFlush () = 0;
	virtual void eventSignal () = 0;
    virtual casRes *lookupRes (const caResId &idIn, casResType type) = 0;
	casEventSys ( const casEventSys & );
	casEventSys & operator = ( const casEventSys & );
};

//
// casClientMon
//
class casClientMon : public casMonitor {
public:
	casClientMon(casChannelI &, caResId clientId,
		const unsigned long count, const unsigned type,
		const casEventMask &maskIn, epicsMutex &mutexIn);
	virtual ~casClientMon();

	caStatus callBack (const smartConstGDDPointer &pValue);

	virtual casResType resourceType() const;

	caResId getId() const
	{
		return this->casRes::getId();
	}

	virtual void destroy();
private:
	casClientMon ( const casClientMon & );
	casClientMon & operator = ( const casClientMon & );
};

class casCtx {
public:
	casCtx();

	//
	// get
	//
	const caHdrLargeArray * getMsg() const;
	void * getData() const;
	caServerI * getServer() const;
	casCoreClient * getClient() const;
	casPVI * getPV() const;
	casChannelI * getChannel() const;

	void setMsg ( caHdrLargeArray &, void * pBody );

	void setServer ( caServerI *p );

	void setClient ( casCoreClient *p );

	void setPV ( casPVI *p );

	 void setChannel ( casChannelI *p );

	void show ( unsigned level ) const;
	
private:
	caHdrLargeArray msg;	// ca message header
	void            *pData; // pointer to data following header
	caServerI       *pCAS;
	casCoreClient   *pClient;
	casChannelI     *pChannel;
	casPVI          *pPV;
	unsigned        nAsyncIO; // checks for improper use of async io
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

class casBufferFactory {
public:
    casBufferFactory ();
    ~casBufferFactory ();
    unsigned smallBufferSize () const;
    char * newSmallBuffer ();
    void destroySmallBuffer ( char * pBuf );
    unsigned largeBufferSize () const;
    char * newLargeBuffer ();
    void destroyLargeBuffer ( char * pBuf );
private:
    void * smallBufFreeList; 
    void * largeBufFreeList; 
    unsigned largeBufferSizePriv;
};

extern epicsSingleton < casBufferFactory > pGlobalBufferFactoryCAS;

class inBufClient {
public:
    enum fillCondition { casFillNone,  casFillProgress,  
        casFillDisconnect };
    // this is a hack for a Solaris IP kernel feature
    enum fillParameter { fpNone, fpUseBroadcastInterface };
    virtual unsigned getDebugLevel () const = 0;
    virtual bufSizeT incomingBytesPresent () const = 0;
    virtual fillCondition xRecv ( char *pBuf, bufSizeT nBytesToRecv, 
        enum fillParameter parm, bufSizeT &nByesRecv ) = 0;
    virtual void hostName ( char *pBuf, unsigned bufSize ) const = 0;
};

//
// inBuf 
//
class inBuf {
    friend class inBufCtx;
public:

    inBuf ( inBufClient &, bufSizeT ioMinSizeIn );
    virtual ~inBuf ();
    
    bufSizeT bytesPresent () const;
    
    bufSizeT bytesAvailable () const;
    
    bool full () const;
    
    //
    // fill the input buffer with any incoming messages
    //
    inBufClient::fillCondition fill ( inBufClient::fillParameter parm = inBufClient::fpNone );
    
    void show ( unsigned level ) const;

    void removeMsg ( bufSizeT nBytes );

    void clear ();

    char * msgPtr () const;

    //
    // This is used to create recursive protocol stacks. A subsegment
    // of the buffer of max size "maxSize" is assigned to the next
    // layer down in the protocol stack by pushCtx () until popCtx ()
    // is called. The roiutine popCtx () returns the actual number
    // of bytes used by the next layer down.
    //
    // pushCtx() returns an outBufCtx to be restored by popCtx()
    //
    const inBufCtx pushCtx ( bufSizeT headerSize, bufSizeT bodySize );
	bufSizeT popCtx ( const inBufCtx & ); // returns actual size

    unsigned bufferSize () const;

    void expandBuffer ();

private:
    epicsMutex  mutex;
    inBufClient & client;
    char        * pBuf;
    bufSizeT    bufSize;
    bufSizeT    bytesInBuffer;
    bufSizeT    nextReadIndex;
    bufSizeT    ioMinSize;
    unsigned    ctxRecursCount;
	inBuf ( const inBuf & );
	inBuf & operator = ( const inBuf & );
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

class outBufClient {
public:
    enum flushCondition { flushNone, flushProgress, flushDisconnect };
	virtual unsigned getDebugLevel () const = 0;
	virtual void sendBlockSignal () = 0;
	virtual flushCondition xSend ( char *pBuf, bufSizeT nBytesAvailableToSend, 
		bufSizeT nBytesNeedToBeSent, bufSizeT &nBytesSent ) = 0;
	virtual void hostName ( char *pBuf, unsigned bufSize ) const = 0;
};

//
// outBuf
//
class outBuf {
    friend class outBufCtx;
public:

	outBuf ( outBufClient & );
	virtual ~outBuf ();

	bufSizeT bytesPresent () const;

	//
	// flush output queue
	// (returns the number of bytes sent)
    //
    outBufClient::flushCondition flush ( bufSizeT spaceRequired=bufSizeT_MAX );

	void show (unsigned level) const;

    unsigned bufferSize () const;

	//
	// allocate message buffer space
	// (leaves message buffer locked)
	//
    caStatus copyInHeader ( ca_uint16_t response, ca_uint32_t payloadSize,
        ca_uint16_t dataType, ca_uint32_t nElem, ca_uint32_t cid, 
        ca_uint32_t responseSpecific, void **pPayload );

    //
    // commit message created with copyInHeader
    //
    void commitMsg ();
	void commitMsg ( ca_uint32_t reducedPayloadSize );

    caStatus allocRawMsg ( bufSizeT msgsize, void **ppMsg );
	void commitRawMsg ( bufSizeT size );

    //
    // This is used to create recursive protocol stacks. A subsegment
    // of the buffer of max size "maxSize" is assigned to the next
    // layer down in the protocol stack by pushCtx () until popCtx ()
    // is called. The roiutine popCtx () returns the actual number
    // of bytes used by the next layer down.
    //
    // pushCtx() returns an outBufCtx to be restored by popCtx()
    //
    const outBufCtx pushCtx ( bufSizeT headerSize, bufSizeT maxBodySize, void *&pHeader );
	bufSizeT popCtx ( const outBufCtx & ); // returns actual size

	void clear ();

private:
    mutable epicsMutex  mutex;
    outBufClient        & client;                 
	char                * pBuf;
	bufSizeT            bufSize;
	bufSizeT            stack;
    unsigned            ctxRecursCount;

    void expandBuffer ();

	outBuf ( const outBuf & );
	outBuf & operator = ( const outBuf & );
};

//
// casCoreClient
// (this will eventually support direct communication
// between the client lib and the server lib)
//
class casCoreClient : public epicsMutex, public ioBlocked, 
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

	virtual caStatus monitorResponse ( casChannelI &chan, const caHdrLargeArray &msg, 
		const smartConstGDDPointer &pDesc, const caStatus status );

	virtual caStatus accessRightsResponse(casChannelI *);

	//
	// one virtual function for each CA request type that has
	// asynchronous completion
	//
	virtual caStatus asyncSearchResponse (
		const caNetAddr & outAddr, 
		const caHdrLargeArray &, const pvExistReturn & );
	virtual caStatus createChanResponse (
		const caHdrLargeArray &, const pvAttachReturn &);
	virtual caStatus readResponse (
		casChannelI *, const caHdrLargeArray &, const smartConstGDDPointer &, const caStatus); 
	virtual caStatus readNotifyResponse (
		casChannelI *, const caHdrLargeArray &, const smartConstGDDPointer &, const caStatus);
	virtual caStatus writeResponse (const caHdrLargeArray &, const caStatus);
	virtual caStatus writeNotifyResponse (const caHdrLargeArray &, const caStatus);

	//
	// used only with DG clients 
	// 
	virtual caNetAddr fetchLastRecvAddr () const;

protected:
	casCtx ctx;
	unsigned char asyncIOFlag;

private:
	tsDLList<casAsyncIOI>   ioInProgList;
	casCoreClient ( const casCoreClient & );
	casCoreClient & operator = ( const casCoreClient & );
};

//
// casClient
//
class casClient : public casCoreClient, public outBufClient, 
    public inBufClient {
public:

	casClient ( caServerI &, bufSizeT ioMinSizeIn );
	virtual ~casClient ();

	virtual void show ( unsigned level ) const;

	caStatus sendErr ( const caHdrLargeArray *, const int reportedStatus,
			const char *pFormat, ... );

    void sendVersion ();

	unsigned getMinorVersion() const {return this->minor_version_number;}

	//
	// find the channel associated with a resource id
	//
	casChannelI * resIdToChannel (const caResId &id);

	virtual void hostName ( char *pBuf, unsigned bufSize ) const = 0;

protected:
    inBuf in;
    outBuf out;
	unsigned minor_version_number;
    unsigned incommingBytesToDrain;
	epicsTime lastSendTS;
	epicsTime lastRecvTS;

	caStatus sendErrWithEpicsStatus ( const caHdrLargeArray *pMsg,
			caStatus epicsStatus, caStatus clientStatus );

#	define logBadId(MP, DP, CACSTAT, RESID) \
	this->logBadIdWithFileAndLineno(MP, DP, CACSTAT, __FILE__, __LINE__, RESID)
	caStatus logBadIdWithFileAndLineno ( const caHdrLargeArray *mp,
			const void *dp, const int cacStat, const char *pFileName, 
			const unsigned lineno, const unsigned resId );

	caStatus processMsg();

	//
	// dump message to stderr
	//
	void dumpMsg ( const caHdrLargeArray *mp, const void *dp, const char *pFormat, ... );


private:
    typedef caStatus ( casClient::*pCASMsgHandler ) ();

	//
	// one function for each CA request type
	//
	virtual caStatus uknownMessageAction () = 0;
	caStatus ignoreMsgAction ();
	virtual caStatus versionAction ();
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

	virtual void userName ( char * pBuf, unsigned bufSize ) const = 0;

	//
	// static members
	//
	static void loadProtoJumpTable();
	static pCASMsgHandler msgHandlers[CA_PROTO_LAST_CMMD+1u];
	static int msgHandlersInit;

	casClient ( const casClient & );
	casClient & operator = ( const casClient & );
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

    void flush ();

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
	virtual caStatus createChanResponse (const caHdrLargeArray &, const pvAttachReturn &);
	caStatus readResponse (casChannelI *pChan, const caHdrLargeArray &msg,
			const smartConstGDDPointer &pDesc, const caStatus status);
	caStatus readNotifyResponse (casChannelI *pChan, const caHdrLargeArray &msg,
			const smartConstGDDPointer &pDesc, const caStatus status);
	caStatus writeResponse (const caHdrLargeArray &msg, const caStatus status);
	caStatus writeNotifyResponse (const caHdrLargeArray &msg, const caStatus status);
	caStatus monitorResponse ( casChannelI & chan, const caHdrLargeArray & msg, 
		const smartConstGDDPointer & pDesc, const caStatus status );

	caStatus noReadAccessEvent( casClientMon * );


	caStatus disconnectChan ( caResId id );

	unsigned getDebugLevel () const;

    virtual void hostName ( char * pBuf, unsigned bufSize ) const = 0;
	void userName ( char * pBuf, unsigned bufSize ) const;

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
	caStatus channelCreateFailed ( const caHdrLargeArray *mp, caStatus createStatus );

	caStatus writeArrayData();
	caStatus writeScalarData();
	caStatus writeString();

	//
	// io independent send/recv
	//
    outBufClient::flushCondition xSend ( char * pBuf, bufSizeT nBytesAvailableToSend,
			bufSizeT nBytesNeedToBeSent, bufSizeT & nBytesSent );
    inBufClient::fillCondition xRecv ( char * pBuf, bufSizeT nBytesToRecv,
			inBufClient::fillParameter parm, bufSizeT & nByesRecv );

	virtual xBlockingStatus blockingState() const = 0;

	virtual outBufClient::flushCondition osdSend ( const char *pBuf, bufSizeT nBytesReq,
			bufSizeT & nBytesActual ) = 0;
	virtual inBufClient::fillCondition osdRecv ( char *pBuf, bufSizeT nBytesReq,
			bufSizeT &nBytesActual ) = 0;

    caStatus readNotifyFailureResponse ( const caHdrLargeArray & msg, 
        const caStatus ECA_XXXX );

    caStatus monitorFailureResponse ( const caHdrLargeArray & msg, 
        const caStatus ECA_XXXX );

	caStatus writeNotifyResponseECA_XXX ( const caHdrLargeArray &msg,
			const caStatus status );

	casStrmClient ( const casStrmClient & );
	casStrmClient & operator = ( const casStrmClient & );
};

class casDGIntfIO;


//
// casDGClient 
//
class casDGClient : public casClient {
public:
	casDGClient (caServerI &serverIn);
	virtual ~casDGClient();

	virtual void show (unsigned level) const;

	//
	// only for use with DG io
	//
	void sendBeacon ( ca_uint32_t beaconNumber );

    virtual void sendBeaconIO ( char &msg, bufSizeT length, 
        aitUint16 &portField, aitUint32 &addrField ) = 0;

	void destroy ();

	unsigned getDebugLevel () const;

    void hostName ( char * pBuf, unsigned bufSize ) const;
	void userName ( char * pBuf, unsigned bufSize ) const;

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
    ca_uint32_t seqNoOfReq;

	//
	// one function for each CA request type
	//
	caStatus searchAction ();
    caStatus uknownMessageAction ();

	//
	// searchFailResponse()
	//
	caStatus searchFailResponse (const caHdrLargeArray *pMsg);

	caStatus searchResponse (const caHdrLargeArray &, const pvExistReturn &);

	caStatus asyncSearchResponse ( const caNetAddr & outAddr, 
		const caHdrLargeArray & msg, const pvExistReturn & );

	//
	// IO depen
	//
	outBufClient::flushCondition xSend ( char *pBufIn, bufSizeT nBytesAvailableToSend, 
		bufSizeT nBytesNeedToBeSent, bufSizeT &nBytesSent );
	inBufClient::fillCondition xRecv ( char *pBufIn, bufSizeT nBytesToRecv, 
        fillParameter parm, bufSizeT &nByesRecv );

	virtual outBufClient::flushCondition osdSend ( const char *pBuf, bufSizeT nBytesReq,
			const caNetAddr & addr ) = 0;
	virtual inBufClient::fillCondition osdRecv ( char *pBuf, bufSizeT nBytesReq,
			fillParameter parm, bufSizeT &nBytesActual, caNetAddr & addr ) = 0;

    caStatus versionAction ();

    //
    // cadg
    //
    struct cadg {
        caNetAddr cadg_addr; // invalid address indicates pad
        bufSizeT cadg_nBytes;
    };

	casDGClient ( const casDGClient & );
	casDGClient & operator = ( const casDGClient & );
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
	casEventRegistry &reg;
	casEventMaskEntry ( const casEventMaskEntry & );
	casEventMaskEntry & operator = ( const casEventMaskEntry & );
};

//
// casEventRegistry
//
class casEventRegistry : private resTable <casEventMaskEntry, stringId> {
    friend class casEventMaskEntry;
public:
    
    casEventRegistry () : maskBitAllocator(0) {}
    
    virtual ~casEventRegistry();
    
    casEventMask registerEvent (const char *pName);
    
    void show (unsigned level) const;
    
private:
    unsigned maskBitAllocator;

    casEventMask maskAllocator();
	casEventRegistry ( const casEventRegistry & );
	casEventRegistry & operator = ( const casEventRegistry & );
};

#include "casIOD.h" // IO dependent
#include "casOSD.h" // OS dependent

class casClientMon;

//
// caServerI
// 
class caServerI : 
	public caServerOS, 
	public caServerIO, 
	public ioBlockedList, 
	private chronIntIdResTable<casRes>,
	public casEventRegistry {
public:
	caServerI ( caServer &tool );
	~caServerI ();

	//
	// find the channel associated with a resource id
	//
	casChannelI *resIdToChannel (const caResId &id);

	//
	// find the PV associated with a resource id
	//
	casPVI *resIdToPV (const caResId &id);

	//
	// find the client monitor associated with a resource id
	//
	casClientMon *resIdToClientMon (const caResId &idIn);

	void installClient (casStrmClient *pClient);

	void removeClient (casStrmClient *pClient);

	//
	// is there space for a new channel
	//
	bool roomForNewChannel() const;

	unsigned getDebugLevel() const { return debugLevel; }
	inline void setDebugLevel (unsigned debugLevelIn);

	void show (unsigned level) const;

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

    void lock () const;
    void unlock () const;

private:
	void advanceBeaconPeriod();

	mutable epicsMutex      mutex;
	tsDLList<casStrmClient> clientList;
    tsDLList<casIntfOS>     intfList;
	double                  maxBeaconInterval;
	double                  beaconPeriod;
	caServer                &adapter;
	unsigned                debugLevel;
    unsigned                nEventsProcessed; 
    unsigned                nEventsPosted; 
    ca_uint32_t             beaconCounter;

    //
    // predefined event types
    //
    casEventMask            valueEvent; // DBE_VALUE registerEvent("value")
	casEventMask            logEvent; 	// DBE_LOG registerEvent("log") 
	casEventMask            alarmEvent; // DBE_ALARM registerEvent("alarm")

	double getBeaconPeriod () const;

	//
	// send beacon and advance beacon timer
	//
	void sendBeacon ();

	caStatus attachInterface (const caNetAddr &addr, bool autoBeaconAddr,
			bool addConfigAddr);

	caServerI ( const caServerI & );
	caServerI & operator = ( const caServerI & );
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

#endif /*INCLserverh*/


