/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casDGIntfIOh
#define casDGIntfIOh

#include "casDGClient.h"
#include "ipIgnoreEntry.h"

class casDGIntfIO : public casDGClient {
public:
	casDGIntfIO ( caServerI & serverIn, clientBufMemoryManager &, 
        const caNetAddr & addr, bool autoBeaconAddr = true, 
        bool addConfigBeaconAddr = false );
	virtual ~casDGIntfIO();

	int getFD () const;
    int getBCastFD () const;
    bool validBCastFD () const;

	void xSetNonBlocking ();
	void sendBeaconIO ( char & msg, bufSizeT length, 
        aitUint16 &portField, aitUint32 & addrField );

	outBufClient::flushCondition osdSend ( const char * pBuf, bufSizeT nBytesReq,
			const caNetAddr & addr);
	inBufClient::fillCondition osdRecv ( char * pBuf, bufSizeT nBytesReq, 
        inBufClient::fillParameter parm, bufSizeT & nBytesActual, caNetAddr & addr );
	virtual void show ( unsigned level ) const;

	static bufSizeT optimumInBufferSize ();

    bufSizeT dgInBytesPending () const;
    bufSizeT osSendBufferSize () const ;

private:
    tsFreeList < ipIgnoreEntry, 128 > ipIgnoreEntryFreeList;
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

inline int casDGIntfIO::getBCastFD() const
{
	return this->bcastRecvSock;
}

inline bool casDGIntfIO::validBCastFD() const
{
	return this->bcastRecvSock != INVALID_SOCKET;
}

#endif // casDGIntfIOh

