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
 * Revision 1.11  1996/09/16 18:24:09  jhill
 * vxWorks port changes
 *
 * Revision 1.10  1996/09/04 20:27:02  jhill
 * doccasdef.h
 *
 * Revision 1.9  1996/08/13 22:56:14  jhill
 * added init for mutex class
 *
 * Revision 1.8  1996/08/05 23:22:58  jhill
 * gddScaler => gddScalar
 *
 * Revision 1.7  1996/08/05 19:27:28  jhill
 * added process()
 *
 * Revision 1.5  1996/07/24 22:00:50  jhill
 * added pushOnToEventQueue()
 *
 * Revision 1.4  1996/07/09 22:51:14  jhill
 * store copy of msg in ctx
 *
 * Revision 1.3  1996/06/26 21:19:04  jhill
 * now matches gdd api revisions
 *
 * Revision 1.2  1996/06/21 02:30:58  jhill
 * solaris port
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */

#ifndef INCLserverh
#define INCLserverh

#ifdef __STDC__
#       define VERSIONID(NAME,VERS) \
                const char *EPICS_CAS_VID_ ## NAME = VERS;
#else /*__STDC__*/
#       define VERSIONID(NAME,VERS) \
                const char *EPICS_CAS_VID_/* */NAME = VERS;
#endif /*__STDC__*/

#if defined(CAS_VERSION_GLOBAL) && 0
#       define HDRVERSIONID(NAME,VERS) VERSIONID(NAME,VERS)
#else /*CAS_VERSION_GLOBAL*/
#       define HDRVERSIONID(NAME,VERS)
#endif /*CAS_VERSION_GLOBAL*/

HDRVERSIONID(serverh, "%W% %G%")

//
// ANSI C
//
#include <stdio.h>

//
// EPICS
//
#define	 epicsAssertAuthor "Jeff Hill johill@lanl.gov"
#include "epicsAssert.h"

//
// CA
//
#include "caCommonDef.h"
#include "caerr.h"
#include "casdef.h"
#include "osiTime.h"

//
// CAS
//
#include "casAddr.h"
#include "osiMutexCAS.h" // NOOP on single threaded OS
void casVerifyFunc(const char *pFile, unsigned line, const char *pExp);
void serverToolDebugFunc(const char *pFile, unsigned line, const char *pComment);
#define serverToolDebug(COMMENT) \
{serverToolDebugFunc(__FILE__, __LINE__, COMMENT); } 
#define casVerify(EXP) {if ((EXP)==0) casVerifyFunc(__FILE__, __LINE__, #EXP); } 
caStatus createDBRDD(unsigned dbrType, aitIndex dbrCount, gdd *&pDescRet);
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
 */
#define individualEventEntries 16u

/*
 * maximum average log entries for each event block (registartion)
 * (events cached into the last queue entry if over flow occurs)
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
public:
	inline casEventSys (casCoreClient &coreClientIn);

	inline caStatus init();

	~casEventSys();

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

	inline aitBool getEventsOff () const;

	inline void setEventsOn();

	inline void setEventsOff();

private:
	tsDLList<casEvent>	eventLogQue;
	osiMutex		mutex;
	casCoreClient		&coreClient;
	unsigned		numEventBlocks;	// N event blocks installed
	unsigned		maxLogEntries; // max log entries
	unsigned char		eventsOff;
};

//
// casClientMon
//
class casClientMon : public casMonitor {
public:
        casClientMon(casChannelI &, caResId clientId,
		const unsigned long count, const unsigned type,
		const casEventMask &maskIn, osiMutex &mutexIn);
        ~casClientMon();

        caStatus callBack(gdd &value);

        casResType resourceType() const
        {
                return casClientMonT;
        }

	caResId getId() const
	{
		return this->casRes::getId();
	}
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
        inline inBuf (osiMutex &, unsigned bufSizeIn);
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
        inline dgInBuf (osiMutex &mutexIn, unsigned bufSizeIn); 
	virtual ~dgInBuf();

        inline void clear();

	inline int hasAddress() const;

	inline const caAddr getSender() const;

        xRecvStatus xRecv (char *pBufIn, bufSizeT nBytesToRecv,
			bufSizeT &nByesRecv);

        virtual xRecvStatus xDGRecv (char *pBuf, bufSizeT nBytesToRecv,
			bufSizeT &nByesRecv, caAddr &sender) = 0;
private:
	casOpaqueAddr	from;
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
        caStatus allocMsg (unsigned extsize, caHdr **ppMsg);

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
        inline dgOutBuf (osiMutex &mutexIn, unsigned bufSizeIn);

        virtual ~dgOutBuf();

	inline caAddr getRecipient();

	inline void setRecipient(const caAddr &addr);

	inline void clear();

        xSendStatus xSend (char *pBufIn, bufSizeT nBytesAvailableToSend, 
			bufSizeT nBytesNeedToBeSent, bufSizeT &nBytesSent);

        virtual xSendStatus xDGSend (char *pBuf, bufSizeT nBytesNeedToBeSent, 
			bufSizeT &nBytesSent, const caAddr &recipient) = 0;
private:
	casOpaqueAddr	to;	
};


//
// casCoreClient
// (this will eventually support direct communication
// between the client lib and the server lib)
//
class casCoreClient : public osiMutex, public ioBlocked, 
	public casEventSys {
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

        virtual caStatus monitorResponse(casChannelI *, 
			const caHdr &, gdd *, const caStatus);

        //
        // one virtual function for each CA request type that has
        // asynchronous completion
        //
        virtual caStatus asyncSearchResponse(casDGIntfIO &outMsgIO,
		const caAddr &outAddr, const caHdr &, const pvExistReturn &);
        virtual caStatus createChanResponse(const caHdr &, 
				const pvExistReturn &);
        virtual caStatus readResponse(casChannelI *, const caHdr &,
                        	gdd *, const caStatus); 
        virtual caStatus readNotifyResponse(casChannelI *, const caHdr &, 
				gdd *, const caStatus);
        virtual caStatus writeResponse(casChannelI *, const caHdr &,
				const caStatus);
        virtual caStatus writeNotifyResponse(casChannelI *, const caHdr &, 
				const caStatus);

	//
	// The following are only used with async IO for
	// DG clients 
	// 
	virtual caAddr fetchRespAddr();
	virtual casDGIntfIO* fetchOutIntf();
protected:
	casCtx			ctx;

private:
	tsDLList<casAsyncIOI>   ioInProgList;

	//
	// static members
	//
	static void loadProtoJumpTable();
	static int msgHandlersInit;
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
        inline casChannelI *resIdToChannel(const caResId &id);

        virtual void clientHostName (char *pBuf, 
			unsigned bufSize) const = 0;

protected:
        unsigned	minor_version_number;
	osiTime		elapsedAtLastSend;
	osiTime		elapsedAtLastRecv;

        caStatus processMsg();


        //
        //
        //
        caStatus sendErrWithEpicsStatus(const caHdr *pMsg,
                        caStatus epicsStatus, caStatus clientStatus);

        //
        // logBadIdWithFileAndLineno()
        //
#	define logBadId(MP, DP) \
	this->logBadIdWithFileAndLineno(MP, DP, __FILE__, __LINE__)
        caStatus logBadIdWithFileAndLineno(const caHdr *mp,
                        const void *dp, const char *pFileName, 
			const unsigned lineno);

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
        ~casStrmClient();

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
        virtual caStatus createChanResponse(const caHdr &, const pvExistReturn &);
        caStatus readResponse(casChannelI *pChan, const caHdr &msg,
                        gdd *pDesc, const caStatus status);
        caStatus readNotifyResponse(casChannelI *pChan, const caHdr &msg,
                        gdd *pDesc, const caStatus status);
        caStatus writeResponse(casChannelI *pChan, const caHdr &msg,
                        const caStatus status);
        caStatus writeNotifyResponse(casChannelI *pChan, const caHdr &msg,
                        const caStatus status);
        caStatus monitorResponse(casChannelI *pChan, const caHdr &msg, 
			gdd *pDesc, const caStatus status);

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
	caStatus read (gdd *&pDesc);
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
};

class casDGIntfIO;


//
// casDGClient 
//
class casDGClient : public dgInBuf, public dgOutBuf, public casClient {
public:
        casDGClient (caServerI &serverIn);

	caStatus init(); //constructor does not return status

        void show (unsigned level) const;

	//
	// only for use with DG io
	//
	static void sendBeacon(casDGIntfIO &io);

	void destroy();

	void processDG(casDGIntfIO &inMsgIO, casDGIntfIO &outMsgIO);

	unsigned getDebugLevel() const;

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

	caStatus asyncSearchResponse(casDGIntfIO &outMsgIO, 
		const caAddr &outAddr, const caHdr &msg, 
		const pvExistReturn &retVal);
	caAddr fetchRespAddr();
	casDGIntfIO* fetchOutIntf();


	//
	// IO depen
	//
        xRecvStatus xDGRecv (char *pBuf, bufSizeT nBytesToRecv,
			bufSizeT &nByesRecv, caAddr &sender);
	xSendStatus xDGSend (char *pBuf, bufSizeT nBytesNeedToBeSent, 
			bufSizeT &nBytesSent, const caAddr &recipient);
	bufSizeT incommingBytesPresent() const;
};

class casEventRegistry : private resTable <casEventMaskEntry, stringId> {
public:
        casEventRegistry(osiMutex &mutexIn) : 
		mutex(mutexIn), allocator(0), hasBeenInitialized(0) {}

	int init();
        ~casEventRegistry()
        {
                this->destroyAllEntries();
        }
        casEventMask registerEvent (const char *pName);
 
        void show (unsigned level);
 
private:
	osiMutex &mutex;
        unsigned allocator;
        unsigned char hasBeenInitialized;
 
        casEventMask maskAllocator();
};

#include "casIOD.h" // IO dependent
#include "casOSD.h" // OS dependent

class casClientMon;

class caServerI : 
	public osiMutex, // osiMutex must be first because it is used
			// by ioBlockedList and casEventRegistry
	public caServerOS, 
	public caServerIO, 
	public ioBlockedList, 
	private uintResTable<casRes>,
	public casEventRegistry {
public:
	caServerI(caServer &tool, unsigned pvMaxNameLength, 
		unsigned pvCountEstimate, unsigned maxSimultaneousIO);
	caStatus init(); //constructor does not return status
	~caServerI();

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

        unsigned getMaxSimultaneousIO() const {return this->maxSimultaneousIO;}

	//
	// install a PV into the server
	//
	inline void installPV(casPVI &pv);

	//
	// remove  PV from the server
	//
	inline void removePV(casPVI &pv);

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
	inline pvExistReturn pvExistTest (const casCtx &ctx, const char *pPVName);
	inline void pvExistTestCompletion();
	inline aitBool pvExistTestPossible();

	casPVI *createPV(const char *pName);

        osiTime getBeaconPeriod() const { return this->beaconPeriod; }

	void show(unsigned level);

	inline casRes *lookupRes(const caResId &idIn, casResType type);

	inline unsigned getPVMaxNameLength() const;

	inline caServer *getAdapter();

	inline void installItem(casRes &res);

	inline casRes *removeItem(casRes &res);

	//
	// call virtual function in the interface class
	//
        inline caServer * operator -> ();

	void connectCB(casIntfOS &);

	inline aitBool ready();

	caStatus addAddr(const caAddr &caAddr, int autoBeaconAddr,
			int addConfigAddr);
private:
        void advanceBeaconPeriod();

        casDGOS				dgClient;
	casCtx				ctx;
	tsDLList<casStrmClient>		clientList;
	tsDLList<casIntfOS>		intfList;
	resTable<casPVI,stringId>	stringResTbl;
        osiTime 			beaconPeriod;
	caServer			&adapter;
	unsigned			pvCount;
        unsigned 			debugLevel;
        unsigned			nExistTestInProg;

        // max number of IO ops pending simultaneously
        // (for operations that are not directed at a particular PV)
        const unsigned 			pvMaxNameLength;

        // the estimated number of proces variables default = ???
        const unsigned 			pvCountEstimate;

        // the maximum number of characters in a pv name
        // default = none - required initialization parameter
        const unsigned 			maxSimultaneousIO;
	
	unsigned char			haveBeenInitialized;
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
 * this really should be in another header file
 */
extern "C" {
void		ca_printf (const char *pFormat, ...);
}

#endif /*INCLserverh*/


