/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
// $Id$
//
// casIOD.h - Channel Access Server BSD socket dependent wrapper
//
//
//

#ifndef includeCASIODH
#define includeCASIODH 

#ifdef epicsExportSharedSymbols
#   define ipIgnoreEntryEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "envDefs.h"
#include "resourceLib.h"
#include "epicsSingleton.h"
#include "tsFreeList.h"
#include "osiSock.h"
#include "inetAddrID.h"

#ifdef ipIgnoreEntryEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

void hostNameFromIPAddr ( const class caNetAddr * pAddr, 
			char * pBuf, unsigned bufSize );

class ipIgnoreEntry : public tsSLNode < ipIgnoreEntry > {
public:
    ipIgnoreEntry ( unsigned ipAddr );
    void destroy ();
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void * pCadaver, size_t size );
    bool operator == ( const ipIgnoreEntry & ) const;
    resTableIndex hash () const;

private:
    unsigned ipAddr;
    static epicsSingleton < tsFreeList < class ipIgnoreEntry, 1024 > > pFreeList;
	ipIgnoreEntry ( const ipIgnoreEntry & );
	ipIgnoreEntry & operator = ( const ipIgnoreEntry & );
};

//
// casDGIntfIO
//
class casDGIntfIO : public casDGClient {
public:
	casDGIntfIO ( caServerI &serverIn, const caNetAddr &addr, 
		bool autoBeaconAddr=TRUE, bool addConfigBeaconAddr = false );
	virtual ~casDGIntfIO();

	int getFD () const;
    int getBCastFD () const;
    bool validBCastFD () const;

	void xSetNonBlocking ();
	void sendBeaconIO ( char & msg, bufSizeT length, 
        aitUint16 &portField, aitUint32 & addrField );
	casIOState state () const;

	outBufClient::flushCondition osdSend ( const char * pBuf, bufSizeT nBytesReq,
			const caNetAddr & addr);
	inBufClient::fillCondition osdRecv ( char * pBuf, bufSizeT nBytesReq, 
        inBufClient::fillParameter parm, bufSizeT & nBytesActual, caNetAddr & addr );
	virtual void show ( unsigned level ) const;

	static bufSizeT optimumOutBufferSize ();
	static bufSizeT optimumInBufferSize ();

    bufSizeT incomingBytesPresent () const;

private:
    resTable < ipIgnoreEntry, ipIgnoreEntry > ignoreTable;
	ELLLIST beaconAddrList;
	SOCKET sock;
	SOCKET bcastRecvSock; // fix for solaris bug
	SOCKET beaconSock; // allow connect
	unsigned short dgPort;

    static SOCKET makeSockDG ();
	casDGIntfIO ( const casDGIntfIO & );
	casDGIntfIO & operator = ( const casDGIntfIO & );
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
	casStreamIO ( caServerI & cas, const ioArgsToNewStreamIO & args );
	~casStreamIO();

	int getFD() const;
	void xSetNonBlocking();

	casIOState state() const;
	void hostName ( char *pBuf, unsigned bufSize ) const;
	
	outBufClient::flushCondition osdSend ( const char *pBuf, bufSizeT nBytesReq, 
		bufSizeT & nBytesActual );
	inBufClient::fillCondition osdRecv ( char *pBuf, bufSizeT nBytesReq, 
		bufSizeT & nBytesActual );

	xBlockingStatus blockingState() const;

	bufSizeT incomingBytesPresent() const;

	static bufSizeT optimumBufferSize ();

	void osdShow ( unsigned level ) const;

	const caNetAddr getAddr() const
	{
		return caNetAddr ( this->addr );
	}

private:
	SOCKET sock;
	struct sockaddr_in addr;
	xBlockingStatus blockingFlag;
	casStreamIO ( const casStreamIO & );
	casStreamIO & operator = ( const casStreamIO & );
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

