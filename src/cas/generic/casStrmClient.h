
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casStrmClienth
#define casStrmClienth

#include "casClient.h"

enum xBlockingStatus { xIsBlocking, xIsntBlocking };

//
// casStrmClient 
//
class casStrmClient : 
    public casClient,
	public tsDLNode < casStrmClient > {
public:
	casStrmClient ( caServerI &, clientBufMemoryManager & );
	virtual ~casStrmClient();
	void show ( unsigned level ) const;
    void flush ();

	//
	// one function for each CA request type that has
	// asynchronous completion
	//
	virtual caStatus createChanResponse ( 
            const caHdrLargeArray &, const pvAttachReturn & );
	caStatus readResponse ( casChannelI * pChan, const caHdrLargeArray & msg,
			const gdd & desc, const caStatus status );
	caStatus readNotifyResponse ( casChannelI *pChan, const caHdrLargeArray & msg,
			const gdd & desc, const caStatus status );
	caStatus writeResponse ( casChannelI &, 
            const caHdrLargeArray & msg, const caStatus status );
	caStatus writeNotifyResponse ( casChannelI &, 
            const caHdrLargeArray &, const caStatus status );
	caStatus monitorResponse ( casChannelI & chan, const caHdrLargeArray & msg, 
		const gdd & desc, const caStatus status );
    caStatus enumPostponedCreateChanResponse ( casChannelI & chan, 
        const caHdrLargeArray & hdr, unsigned dbrType );
	caStatus channelCreateFailedResp ( const caHdrLargeArray &, 
        const caStatus createStatus );

	caStatus disconnectChan ( caResId id );
	unsigned getDebugLevel () const;
    virtual void hostName ( char * pBuf, unsigned bufSize ) const = 0;
	void userName ( char * pBuf, unsigned bufSize ) const;

private:
	chronIntIdResTable < casChannelI > chanTable;
	tsDLList < casChannelI > chanList;
	char * pUserName;
	char * pHostName;

	//
	// createChannel()
	//
	caStatus createChannel ( const char *pName );

	//
	// verify read/write requests
	//
	caStatus verifyRequest ( casChannelI * & pChan );

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
	caStatus accessRightsResponse ( casChannelI * pciu );

	//
	// these prepare the gdd based on what is in the ca hdr
	//
	caStatus read ( const gdd * & pDesc );
	caStatus write ();

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

	virtual xBlockingStatus blockingState () const = 0;

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

	caStatus casMonitorCallBack ( casMonitor &,
        const gdd & );

	casStrmClient ( const casStrmClient & );
	casStrmClient & operator = ( const casStrmClient & );
};

#endif // casStrmClienth

