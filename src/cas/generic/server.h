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
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

//
// EPICS
//
#define	 epicsAssertAuthor "Jeff Hill johill@lanl.gov"
#include <epicsAssert.h>
#include <db_access.h>

//
// CA
//
#include <caCommonDef.h>
#include <caerr.h>
#include <casdef.h>
#include <osiTime.h>

//
// gdd
//
#if 0
#include <dbMapper.h>
#include <gddApps.h>
#endif

//
// CAS
//
void casVerifyFunc(const char *pFile, unsigned line, const char *pExp);
void serverToolDebugFunc(const char *pFile, unsigned line);
#define serverToolDebug() {serverToolDebugFunc(__FILE__, __LINE__); } 
#define casVerify(EXP) {if ((EXP)==0) casVerifyFunc(__FILE__, __LINE__, #EXP); } 
caStatus createDBRDD(unsigned dbrType, aitIndex dbrCount, gdd *&pDescRet);
caStatus copyBetweenDD(gdd &dest, gdd &src);

//
// casMsgIO 
//
enum xRecvStatus {xRecvOK, xRecvDisconnect};
enum xSendStatus {xSendOK, xSendDisconnect};
enum casIOState {casOnLine, casOffLine};
typedef unsigned bufSizeT;
class casMsgIO {
public:
        casMsgIO();
	virtual ~casMsgIO();

	osiTime timeOfLastXmit() const;
	osiTime timeOfLAstRecv() const;

        //
        // show status of IO subsystem
	// (cant be const because a lock is taken)
        //
        void show (unsigned level);

	//
	// device dependent recv
	//
        xSendStatus xSend (char *pBuf, bufSizeT nBytesToSend, 
			bufSizeT &nBytesSent);
        xRecvStatus xRecv (char *pBuf, bufSizeT nBytesToRecv,
			bufSizeT &nByesRecv);

        virtual caStatus init()=0;
        virtual bufSizeT optimumBufferSize ()=0;
	virtual bufSizeT incommingBytesPresent() const;
        virtual casIOState state() const=0;
        virtual void hostNameFromAddr (char *pBuf, unsigned bufSize)=0;
	virtual int getFileDescriptor() const;
	virtual void setNonBlocking();

	//
	// only for use with DG io
	//
	virtual void sendBeacon(char &msg, bufSizeT length,
		aitUint32 &m_avail);

private:
	//
	// private data members
	//
        osiTime	elapsedAtLastSend;
        osiTime	elapsedAtLastRecv;

        virtual xSendStatus osdSend (const char *pBuf,
                bufSizeT nBytesReq, bufSizeT &nBytesActual) =0;
        virtual xRecvStatus osdRecv (char *pBuf,
                bufSizeT nBytesReq, bufSizeT &nBytesActual) =0;
        virtual void osdShow (unsigned level) const = 0;
};

#include <casIOD.h> // IO dependent
#include <casOSD.h> // OS dependent

enum casProcCond {casProcOk, casProcDisconnect};


/*
 * maximum peak log entries for each event block (registartion)
 * (events cached into the last queue entry if over flow occurs)
 */
const unsigned char individualEventEntries = 16u;

/*
 * maximum average log entries for each event block (registartion)
 * (events cached into the last queue entry if over flow occurs)
 */
const unsigned char averageEventEntries = 4u;

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
	casEventSys (casCoreClient &coreClientIn) :
		coreClient(coreClientIn),
		numEventBlocks(0u),
		maxLogEntries(individualEventEntries),
		eventsOff(aitFalse)
	{
	}
	~casEventSys();

	void show(unsigned level);
	casProcCond process();
	//void cancelIODone (casAsyncIO *pEventToDelete);


	void installMonitor();
	void removeMonitor();

	void removeFromEventQueue(casEvent &);
	inline void addToEventQueue(casEvent &);

	void insertEventQueue(casEvent &insert, casEvent &prevEvent);
	void pushOnToEventQueue(casEvent &event);

	aitBool full();

	inline casMonitor *resIdToMon(const caResId id);

	casCoreClient &getCoreClient()
	{
		return this->coreClient;
	}

	aitBool getEventsOff () const
	{
		return this->eventsOff?aitTrue:aitFalse;
	}

	void setEventsOn()
	{
		this->eventsOff = aitFalse;
	}

	void setEventsOff()
	{
		this->eventsOff = aitTrue;
	}
private:
	tsDLList<casEvent>	eventLogQue;
	osiMutex		mutex;
	casCoreClient		&coreClient;
	unsigned		numEventBlocks;	// N event blocks installed
	unsigned		maxLogEntries; // max log entries
	unsigned char		eventsOff;
};

//
// casEventSys::insertEventQueue()
//
inline void casEventSys::insertEventQueue(casEvent &insert, casEvent &prevEvent)
{
	this->mutex.lock();
	this->eventLogQue.insertAfter(insert, prevEvent);
	this->mutex.unlock();
}

//
// casEventSys::pushOnToEventQueue()
//
inline void casEventSys::pushOnToEventQueue(casEvent &event)
{
	this->mutex.lock();
	this->eventLogQue.push(event);
	this->mutex.unlock();
}

//
// casEventSys::removeFromEventQueue()
//
inline void casEventSys::removeFromEventQueue(casEvent &event)
{
	this->mutex.lock();
        this->eventLogQue.remove(event);
	this->mutex.unlock();
}

//
// casEventSys::full()
//
inline aitBool casEventSys::full()
{
	if (this->eventLogQue.count()>=this->maxLogEntries) {
		return aitTrue;
	}
	else {
		return aitFalse;
	}
}

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

//
// ndim == 0 => scaler
// otherwise pIndexArray points to an array of ndim items
//
#define nDimScaler 0U
class casCtx {
public:
	casCtx() : 
        	pData(NULL), pCAS(NULL), pClient(NULL),
		pChannel(NULL), pPV(NULL) 
	{
		memset(&this->msg, 0, sizeof(this->msg));
	}

        //
        // get
        //
        const caHdr *getMsg() const {return (const caHdr *) &this->msg;};
        void *getData() const {return this->pData;};
        caServerI * getServer() const {return this->pCAS;}
        casCoreClient * getClient() const {return this->pClient;}
        casPVI * getPV() const {return this->pPV;}
        casChannelI * getChannel() const {return this->pChannel;}

        //
        // set
	// (assumes incoming message is in network byte order)
        //
        void setMsg(const char *pBuf) 
	{
		//
		// copy as raw bytes in order to avoid
		// alignment problems
		//
		memcpy (&this->msg, pBuf, sizeof(this->msg));
                this->msg.m_cmmd = ntohs (this->msg.m_cmmd);
                this->msg.m_postsize = ntohs (this->msg.m_postsize);
                this->msg.m_type = ntohs (this->msg.m_type);
                this->msg.m_count = ntohs (this->msg.m_count);
                this->msg.m_cid = ntohl (this->msg.m_cid);
                this->msg.m_available = ntohl (this->msg.m_available);
	};
        void setData(void *p) {this->pData = p;};
        void setServer(caServerI *p) 
	{
		this->pCAS = p;
	}
        void setClient(casCoreClient *p) {
		this->pClient = p;
	}
        void setPV(casPVI *p) {this->pPV = p;}
        void setChannel(casChannelI *p) {this->pChannel = p;}

	void show (unsigned level)
	{
		printf ("casCtx at %x\n", (unsigned) this);
		if (level >= 1u) {
			printf ("\tpMsg = %x\n", (unsigned) &this->msg);
			printf ("\tpData = %x\n", (unsigned) pData);
			printf ("\tpCAS = %x\n", (unsigned) pCAS);
			printf ("\tpClient = %x\n", (unsigned) pClient);
			printf ("\tpChannel = %x\n", (unsigned) pChannel);
			printf ("\tpPV = %x\n", (unsigned) pPV);
		}
	}
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
        inBuf(casMsgIO &, osiMutex &);
	caStatus init(); //constructor does not return status
	virtual ~inBuf();

        bufSizeT bytesPresent() const {
		return this->bytesInBuffer-this->nextReadIndex;
        }

        bufSizeT bytesAvailable() const {
		bufSizeT bp;
		bufSizeT ibp;
		bp = this->bytesPresent();
		ibp = this->io.incommingBytesPresent();
                return bp + ibp;
        }

        aitBool full() const 
	{
		if (this->bytesPresent()>=this->bufSize) {
			return aitTrue;
		}
		return aitFalse;
	}

        void clear()
	{
		this->bytesInBuffer = 0u;
		this->nextReadIndex = 0u;
	}

        //
        //
        //
        char *msgPtr() const {return &this->pBuf[this->nextReadIndex];}

	//
	//
	//
        void removeMsg(unsigned nBytes) {
                this->nextReadIndex += nBytes;
                assert(this->nextReadIndex<=this->bytesInBuffer);
        }

	//
	// fill the input buffer with any incoming messages
	//
	casFillCondition fill();

	void show(unsigned level);

	virtual unsigned getDebugLevel()=0;
private:
	osiMutex	&mutex;
        casMsgIO 	&io;
        char            *pBuf;
        bufSizeT        bufSize;
        bufSizeT        bytesInBuffer;
        bufSizeT        nextReadIndex;
};

//
// outBuf
//
enum casFlushCondition{
	casFlushNone, 
	casFlushPartial, 
	casFlushCompleted,
	casFlushDisconnect};

class outBuf {
public:
        outBuf (casMsgIO &, osiMutex &);
	caStatus init(); //constructor does not return status
        virtual ~outBuf();

	//
	// number of bytes in the output queue?
	//
	bufSizeT bytesPresent() const 
	{ return this->stack; }

        //
        // flush output queue
	// (returns the number of bytes sent)
        //
        casFlushCondition flush();

        //
        // allocate message buffer space
        // (leaves message buffer locked)
        //
        caStatus allocMsg (unsigned extsize, caHdr **ppMsg);

        //
        // commits message allocated with allocMsg()
        //
        void commitMsg ();

        //
        // release an allocated message (but dont send it)
        //
        void discardMsg () { this->mutex.unlock(); };

	void show(unsigned level);

	virtual unsigned getDebugLevel()=0;
	virtual void sendBlockSignal()=0;

private:
        casMsgIO 	&io;
	osiMutex	&mutex;
        char            *pBuf;
        bufSizeT        bufSize;
        bufSizeT        stack;
};



//
// casCoreClient
// (this will eventually support direct communication
// between the client lib and the server lib)
//
class casCoreClient;

typedef caStatus (casCoreClient::*pAsyncIoCallBack)
	(casChannelI *pChan, const caHdr &, gdd *, const caStatus);

class casCoreClient : public osiMutex, public ioBlocked, 
	public casEventSys {
public:
	casCoreClient(caServerI &serverInternal); 
	virtual ~casCoreClient();
	virtual void destroy();
	virtual caStatus disconnectChan(caResId id);
	virtual void eventSignal() = 0;
	virtual void eventFlush() = 0;
        virtual caStatus start () = 0;
	virtual void show (unsigned level);
	virtual void installChannel (casChannelI &);
	virtual void removeChannel (casChannelI &);

	void installAsyncIO(casAsyncIOI &ioIn)
	{
		this->lock();
		this->ioInProgList.add(ioIn);
		this->unlock();
	}

	void removeAsyncIO(casAsyncIOI &ioIn)
	{
		this->lock();
		this->ioInProgList.remove(ioIn);
		this->unlock();
	}

	casRes *lookupRes(const caResId &idIn, casResType type);

	caServerI &getCAS() const {return *this->ctx.getServer();}

	caStatus asyncIOCompletion(casChannelI *pChan, const caHdr &msg, 
			gdd *pDesc, caStatus completionStatus);

        virtual caStatus monitorResponse(casChannelI *, 
			const caHdr &, gdd *, const caStatus);

protected:
	casCtx			ctx;

private:
	tsDLList<casAsyncIOI>   ioInProgList;

        //
        // one virtual function for each CA request type that has
        // asynchronous completion
        //
        virtual caStatus searchResponse(casChannelI *, const caHdr &,
                        gdd *, const caStatus);
        virtual caStatus createChanResponse(casChannelI *, 
			const caHdr &, gdd *, const caStatus);
        virtual caStatus readResponse(casChannelI *, const caHdr &,
                        gdd *, const caStatus); 
        virtual caStatus readNotifyResponse(casChannelI *, 
		const caHdr &, gdd *, const caStatus);
        virtual caStatus writeResponse(casChannelI *, const caHdr &,
			gdd *, const caStatus);
        virtual caStatus writeNotifyResponse(casChannelI *, 
			const caHdr &, gdd *, const caStatus);

	//
	// static members
	//
	static void loadProtoJumpTable();
        static pAsyncIoCallBack asyncIOJumpTable[CA_PROTO_LAST_CMMD+1u];
	static int msgHandlersInit;
};

//
// casClient
//
class casClient;
typedef caStatus (casClient::*pCASMsgHandler) ();
class casClient : public inBuf, public outBuf, public casCoreClient {
public:
        casClient (caServerI &, casMsgIO &);
	caStatus init(); //constructor does not return status
        virtual ~casClient ();

	void show(unsigned level);

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

	inline unsigned getDebugLevel();

	int getFD() const
	{
		return this->msgIO.getFileDescriptor();
	}
	void setNonBlocking()
	{
		this->msgIO.setNonBlocking();
	}
	void sendBeacon(char &msg, bufSizeT length,
		aitUint32 &m_avail)
	{
		this->msgIO.sendBeacon(msg, length, m_avail);
	}
protected:
        unsigned	minor_version_number;

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
	casMsgIO		&msgIO;

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
class casStrmClient : public casClient, public tsDLNode<casStrmClient> {
public:
        casStrmClient (caServerI &, casMsgIO &);
	caStatus init(); //constructor does not return status
        ~casStrmClient();

        void show(unsigned level);

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
        caStatus createChanResponse(casChannelI *pChan, const caHdr &msg, 
			gdd *pDesc, const caStatus status);
        caStatus readResponse(casChannelI *pChan, const caHdr &msg,
                        gdd *pDesc, const caStatus status);
        caStatus readNotifyResponse(casChannelI *pChan, const caHdr &msg,
                        gdd *pDesc, const caStatus status);
        caStatus writeResponse(casChannelI *pChan, const caHdr &msg,
                        gdd *pDesc, const caStatus status);
        caStatus writeNotifyResponse(casChannelI *pChan, const caHdr &msg,
                        gdd *pDesc, const caStatus status);
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
private:
        tsDLList<casChannelI>	chanList;
        char                    *pUserName;
        char                    *pHostName;
        struct cas_io_in_prog   *ioBlockCache;

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
	caStatus writeScalerData();
	caStatus writeString();
};


//
// casDGClient 
//
class casDGClient : private casDGIO, public casClient {
public:
        casDGClient (caServerI &serverIn);

	caStatus init() //constructor does not return status
	{
		caStatus status;

		//
		// init the base classes
		//
		status = casDGIO::init();
		if (status) {
			return status;
		}
		status = this->casClient::init();
		if (status) {
			return status;
		}
		return this->start();
	}

        void show(unsigned level);

	//
	// only for use with DG io
	//
	void sendDGBeacon(char &msg, bufSizeT length,
		aitUint32 &m_avail)
	{
		this->casClient::sendBeacon(msg, length, m_avail);
	}

	void destroy();
private:
	void ioBlockedSignal(); // dummy

        //
        // one function for each CA request type
        //
        caStatus searchAction ();

        //
        // searchFailResponse()
        //
        caStatus searchFailResponse(const caHdr *pMsg);

	caStatus searchResponse(casChannelI *pChan, const caHdr &msg,
                        gdd *pDesc, const caStatus status);
};

#include <casClientOS.h> // OS dependent

class casClientMon;

//
// caServerI
//
class caServerI : public caServerOS, public caServerIO, 
	public osiMutex, public ioBlockedList,
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
        // find the async IO associated with a resource id
        //
        casAsyncIO *resIdToAsyncIO(const caResId &id);

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
	// only for use with DG io
	// 
	void sendBeacon();

	unsigned getDebugLevel() const { return debugLevel; }
	void setDebugLevel(unsigned debugLevelIn) 
	{ 
		this->debugLevel = debugLevelIn; 
	}
	
	inline caStatus pvExistTest (const casCtx &ctx, const char *pPVName,
					gdd &canonicalPVName);
	inline void pvExistTestCompletion();
	inline aitBool pvExistTestPossible();

	casPVI *createPV(/* const */gdd &name);

        osiTime getBeaconPeriod() const { return this->beaconPeriod; }

	void show(unsigned level);

	inline casRes *lookupRes(const caResId &idIn, casResType type);

	unsigned getPVMaxNameLength() const 
	{
		return this->pvMaxNameLength;
	}

	caServer *getAdapter() 
	{ 
		return &this->adapter; 
	}

	uintResTable<casRes> &getResTable() {return *this;}

	void installItem(casRes &res)
	{
		this->uintResTable<casRes>::installItem(res);
	}

	casRes *removeItem(casRes &res)
	{
		return this->uintResTable<casRes>::remove(res);
	}

	//
	// call virtual function in the interface class
	//
        caServer * operator -> ()
        {
                return this->getAdapter();
        }

	void connectCB();

private:
        void advanceBeaconPeriod();

        casDGOS				dgClient;
	casCtx				ctx;
	tsDLList<casStrmClient>		clientList;
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
};

#define CAServerConnectPendQueueSize 10
const osiTime CAServerMaxBeaconPeriod (5.0 /* sec */);
const osiTime CAServerMinBeaconPeriod (1.0e-3 /* sec */);

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


