

/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casDGClienth
#define casDGClienth

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casDGClienth
#   undef epicsExportSharedSymbols
#endif

#include "epicsTime.h"

#ifdef epicsExportSharedSymbols_casDGClienth
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "casCoreClient.h"
#include "inBuf.h"
#include "outBuf.h"

class casDGClient : public casCoreClient, public outBufClient, 
    public inBufClient {
public:
	casDGClient ( class caServerI & serverIn, 
        clientBufMemoryManager & );
	virtual ~casDGClient ();
	caStatus processMsg ();
	void show ( unsigned level ) const;
	void sendBeacon ( ca_uint32_t beaconNumber );
    virtual void sendBeaconIO ( char & msg, bufSizeT length, 
        aitUint16 & portField, aitUint32 & addrField ) = 0;
	void destroy ();
	unsigned getDebugLevel () const;
    void hostName ( char * pBuf, unsigned bufSize ) const;
	caNetAddr fetchLastRecvAddr () const;
    virtual caNetAddr serverAddress () const = 0;
    caStatus sendErr ( const caHdrLargeArray * curp, 
        ca_uint32_t cid, const int reportedStatus, 
        const char *pformat, ... );
    caStatus processDG ();
protected:
    bool inBufFull () const;
    void inBufFill ( inBufClient::fillParameter );
    bufSizeT inBufBytesPending () const;
    bufSizeT outBufBytesPending () const;
    outBufClient::flushCondition flush ();
private:
    inBuf in;
    outBuf out;
    caNetAddr lastRecvAddr;
	epicsTime lastSendTS;
	epicsTime lastRecvTS;
    ca_uint32_t seqNoOfReq;
	ca_uint16_t minor_version_number;

    typedef caStatus ( casDGClient :: * pCASMsgHandler ) ();
	static pCASMsgHandler const msgHandlers[CA_PROTO_LAST_CMMD+1u];

	// one function for each CA request type
	caStatus searchAction ();
    caStatus uknownMessageAction ();
    caStatus echoAction ();

	caStatus searchFailResponse ( const caHdrLargeArray *pMsg );
	caStatus searchResponse ( const caHdrLargeArray &,
                                const pvExistReturn & retVal );
	caStatus asyncSearchResponse (
        epicsGuard < casClientMutex > &, const caNetAddr & outAddr, 
		const caHdrLargeArray &, const pvExistReturn &,
        ca_uint16_t protocolRevision, ca_uint32_t sequenceNumber );
    void sendVersion ();
	outBufClient::flushCondition xSend ( char *pBufIn, bufSizeT nBytesToSend, 
		                                    bufSizeT &nBytesSent );
	inBufClient::fillCondition xRecv ( char * pBufIn, bufSizeT nBytesToRecv, 
        fillParameter parm, bufSizeT & nByesRecv );
	virtual outBufClient::flushCondition osdSend ( 
            const char * pBuf, bufSizeT nBytesReq, const caNetAddr & addr ) = 0;
	virtual inBufClient::fillCondition osdRecv ( char *pBuf, bufSizeT nBytesReq,
			fillParameter parm, bufSizeT &nBytesActual, caNetAddr & addr ) = 0;
    caStatus versionAction ();
    ca_uint32_t datagramSequenceNumber () const;
	ca_uint16_t protocolRevision () const;
    struct cadg {
        caNetAddr cadg_addr; // invalid address indicates pad
        bufSizeT cadg_nBytes;
    };
	casDGClient ( const casDGClient & );
	casDGClient & operator = ( const casDGClient & );
};

inline ca_uint16_t casDGClient::protocolRevision () const 
{
    return this->minor_version_number;
}


#endif // casDGClienth

