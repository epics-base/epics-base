

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

#include "casClient.h"

class casDGClient : public casClient {
public:
	casDGClient ( class caServerI & serverIn, 
        clientBufMemoryManager & );
	virtual ~casDGClient ();

	virtual void show (unsigned level) const;

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
	caStatus searchFailResponse ( const caHdrLargeArray *pMsg );

	caStatus searchResponse ( const caHdrLargeArray &,
                                const pvExistReturn & retVal );

    caStatus asyncSearchResponse ( const caNetAddr & outAddr,
	    const caHdrLargeArray & msg, const pvExistReturn & retVal,
        ca_uint16_t protocolRevision, ca_uint32_t sequenceNumber );

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

    ca_uint32_t datagramSequenceNumber () const;

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

#endif // casDGClienth

