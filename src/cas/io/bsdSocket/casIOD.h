//
// $Id$
//
// casIOD.h - Channel Access Server BSD socket dependent wrapper
//
//
//

#ifndef includeCASIODH
#define includeCASIODH 

#undef epicsExportSharedSymbols
#include "envDefs.h"
#define epicsExportSharedSymbols

void hostNameFromIPAddr (const caNetAddr *pAddr, 
			char *pBuf, unsigned bufSize);

class casDGClient;

//
// casDGIntfIO
//
class casDGIntfIO {
public:
	casDGIntfIO (casDGClient &clientIn);
	caStatus init(const caNetAddr &addr, unsigned connectWithThisPort,
		int autoBeaconAddr=TRUE, int addConfigBeaconAddr=FALSE, 
		int useBroadcastAddr=FALSE, casDGIntfIO *pAltOutIn=NULL);
	virtual ~casDGIntfIO();

	int getFD() const;
	void xSetNonBlocking();
	void sendBeacon(char &msg, bufSizeT length, aitUint32 &m_ipa, aitUint16 &m_port);
	casIOState state() const;
	void clientHostName (char *pBuf, unsigned bufSize) const;
	
	xSendStatus osdSend (const char *pBuf, bufSizeT nBytesReq, 
		bufSizeT &nBytesActual, const caNetAddr &addr);
	xRecvStatus osdRecv (char *pBuf, bufSizeT nBytesReq, 
		bufSizeT &nBytesActual, caNetAddr &addr);
	void osdShow (unsigned level) const;

	static bufSizeT optimumOutBufferSize ();
	static bufSizeT optimumInBufferSize ();

	//
	// The server's port number
	// (to be used for connection requests)
	//
	unsigned serverPortNumber();

	virtual caStatus start()=0; // OS specific

	void processDG();

private:
	ELLLIST beaconAddrList;
    struct sockaddr lastRecvAddr;
	casDGIntfIO *pAltOutIO;
	casDGClient &client;
	SOCKET sock;
	casIOState sockState;
	unsigned short connectWithThisPort;
	unsigned short dgPort;
};

struct ioArgsToNewStreamIO {
	caNetAddr addr;
	SOCKET sock;
};

//
// casDGIO 
//
class casDGIO : public casDGClient {
public:
	casDGIO(caServerI &cas) :
		casDGClient(cas) {}
	~casDGIO();

	//
	// clientHostName()
	//
	void clientHostName (char *pBufIn, unsigned bufSizeIn) const;
};

//
// casStreamIO
//
class casStreamIO : public casStrmClient {
public:
	casStreamIO(caServerI &cas, const ioArgsToNewStreamIO &args);
	caStatus init();
	~casStreamIO();

	int getFD() const;
	void xSetNonBlocking();

	casIOState state() const;
	void clientHostName (char *pBuf, unsigned bufSize) const;
	
	xSendStatus osdSend (const char *pBuf, bufSizeT nBytesReq, 
		bufSizeT &nBytesActual);
	xRecvStatus osdRecv (char *pBuf, bufSizeT nBytesReq, 
		bufSizeT &nBytesActual);

	xBlockingStatus blockingState() const;

	bufSizeT incommingBytesPresent() const;

	static bufSizeT optimumBufferSize ();

	void osdShow (unsigned level) const;

	//
	// The server's port number
	// (to be used for connection requests)
	//
	unsigned serverPortNumber();

	const caNetAddr getAddr()
	{
		return caNetAddr(this->addr);
	}
private:
	casIOState sockState;
	SOCKET sock;
	struct sockaddr_in addr;
	xBlockingStatus blockingFlag;
};

class caServerIO;
class casDGClient;

class casStreamOS;

//
// casIntfIO
//
class casIntfIO {
public:
	casIntfIO();
	//constructor does not return status
	caStatus init(const caNetAddr &addr, casDGClient &dgClientIn,
		int autoBeaconAddr, int addConfigBeaconAddr); 
	virtual ~casIntfIO();
	void show(unsigned level) const;

	unsigned portNumber() const;

	int getFD() const;

	void setNonBlocking();

	// 
	// called when we expect that a virtual circuit for a
	// client can be created
	// 
	casStreamOS *newStreamClient(caServerI &cas) const;
	
	virtual casDGIntfIO *newDGIntfIO (casDGClient &dgClientIn) const = 0;

	void requestBeacon();
private:
	casDGIntfIO *pNormalUDP;	// attached to this intf's addr
	casDGIntfIO *pBCastUDP;	// attached to this intf's broadcast addr
	SOCKET sock;
	struct sockaddr_in addr;
};

//
// caServerIO
//
class caServerIO {
public:
	caServerIO() {} // make hp compiler happy
	caStatus init(caServerI &cas); //constructor does not return status
	virtual ~caServerIO();

	//
	// show status of IO subsystem
	//
	void show (unsigned level) const;

private:

	//
	// static member data
	//
	static int staticInitialized;
	//
	// static member func
	//
	static inline void staticInit();
};


// no additions below this line
#endif // includeCASIODH

