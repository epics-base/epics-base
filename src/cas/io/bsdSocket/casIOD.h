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
class casDGIntfIO : public casDGClient {
public:
	casDGIntfIO (caServerI &serverIn, const caNetAddr &addr, 
		bool autoBeaconAddr=TRUE, bool addConfigBeaconAddr=FALSE);
	virtual ~casDGIntfIO();

	int getFD () const;
    int getBCastFD () const;
    bool validBCastFD () const;

	void xSetNonBlocking ();
	void sendBeaconIO (char &msg, bufSizeT length, aitUint16 &m_port);
	casIOState state () const;

	outBuf::flushCondition osdSend (const char *pBuf, bufSizeT nBytesReq,
			const caNetAddr &addr);
	inBuf::fillCondition osdRecv (char *pBuf, bufSizeT nBytesReq, 
        fillParameter parm, bufSizeT &nBytesActual, caNetAddr &addr);
	void osdShow (unsigned level) const;

	static bufSizeT optimumOutBufferSize ();
	static bufSizeT optimumInBufferSize ();

    bufSizeT incommingBytesPresent () const;

private:
	ELLLIST beaconAddrList;
	SOCKET sock;
	SOCKET bcastRecvSock; // fix for solaris bug
	unsigned short dgPort;

    static SOCKET makeSockDG ();
};

struct ioArgsToNewStreamIO {
	caNetAddr addr;
	SOCKET sock;
};

//
// casStreamIO
//
class casStreamIO : public casStrmClient {
public:
	casStreamIO(caServerI &cas, const ioArgsToNewStreamIO &args);
	~casStreamIO();

	int getFD() const;
	void xSetNonBlocking();

	casIOState state() const;
	void clientHostName (char *pBuf, unsigned bufSize) const;
	
	outBuf::flushCondition osdSend (const char *pBuf, bufSizeT nBytesReq, 
		bufSizeT &nBytesActual);
	inBuf::fillCondition osdRecv (char *pBuf, bufSizeT nBytesReq, 
		bufSizeT &nBytesActual);

	xBlockingStatus blockingState() const;

	bufSizeT incommingBytesPresent() const;

	static bufSizeT optimumBufferSize ();

	void osdShow (unsigned level) const;

	const caNetAddr getAddr() const
	{
		return caNetAddr(this->addr);
	}

private:
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
	casIntfIO (const caNetAddr &addr);
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

    caNetAddr serverAddress () const;

private:
	SOCKET sock;
	struct sockaddr_in addr;
};

//
// caServerIO
//
class caServerIO {
public:
	caServerIO ();
	virtual ~caServerIO();

	//
	// show status of IO subsystem
	//
	void show (unsigned level) const;

    void locateInterfaces ();

private:

	//
	// static member data
	//
	static int staticInitialized;
	//
	// static member func
	//
	static inline void staticInit();

	virtual caStatus attachInterface (const caNetAddr &addr, bool autoBeaconAddr,
			bool addConfigAddr) = 0;
};


// no additions below this line
#endif // includeCASIODH

