/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casStrmClienth
#define casStrmClienth

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casStrmClienth
#   undef epicsExportSharedSymbols
#endif

#include "epicsTime.h"

#ifdef epicsExportSharedSymbols_casStrmClienth
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "casCoreClient.h"
#include "inBuf.h"
#include "outBuf.h"

enum xBlockingStatus { xIsBlocking, xIsntBlocking };

//
// casStrmClient 
//
class casStrmClient : 
    public casCoreClient, public outBufClient, 
    public inBufClient, public tsDLNode < casStrmClient > {
public:
    casStrmClient ( caServerI &, clientBufMemoryManager &, const caNetAddr & clientAddr );
    virtual ~casStrmClient();
    void show ( unsigned level ) const;
    outBufClient::flushCondition flush ();
    unsigned getDebugLevel () const;
    void hostName ( char * pBuf, unsigned bufSize ) const;
    void userName ( char * pBuf, unsigned bufSize ) const;
    ca_uint16_t protocolRevision () const;
    void sendVersion ();
protected:
    caStatus processMsg ();
    bool inBufFull () const;
    inBufClient::fillCondition inBufFill ();
    bufSizeT inBufBytesPending () const;
    bufSizeT outBufBytesPending () const;
private:
    //char hostNameStr [32];
    inBuf in;
    outBuf out;
    chronIntIdResTable < casChannelI > chanTable;
    tsDLList < casChannelI > chanList;
    epicsTime lastSendTS;
    epicsTime lastRecvTS;
    caNetAddr _clientAddr;
    char * pUserName;
    char * pHostName;
    smartGDDPointer pValueRead;
    unsigned incommingBytesToDrain;
    caStatus pendingResponseStatus;
    ca_uint16_t minor_version_number;
    bool reqPayloadNeedsByteSwap;
    bool responseIsPending;

    caStatus createChannel ( const char * pName );
    caStatus verifyRequest ( casChannelI * & pChan );
    typedef caStatus ( casStrmClient :: * pCASMsgHandler ) 
        ( epicsGuard < casClientMutex > & );
    static pCASMsgHandler const msgHandlers[CA_PROTO_LAST_CMMD+1u];

	//
	// one function for each CA request type
	//
    caStatus uknownMessageAction ( epicsGuard < casClientMutex > & );
    caStatus ignoreMsgAction ( epicsGuard < casClientMutex > & );
    caStatus versionAction ( epicsGuard < casClientMutex > & );
    caStatus echoAction ( epicsGuard < casClientMutex > & );
    caStatus eventAddAction ( epicsGuard < casClientMutex > & );
    caStatus eventCancelAction ( epicsGuard < casClientMutex > & );
    caStatus readAction ( epicsGuard < casClientMutex > & );
    caStatus readNotifyAction ( epicsGuard < casClientMutex > & );
    caStatus writeAction ( epicsGuard < casClientMutex > & );
    caStatus eventsOffAction ( epicsGuard < casClientMutex > & );
    caStatus eventsOnAction ( epicsGuard < casClientMutex > & );
    caStatus readSyncAction ( epicsGuard < casClientMutex > & );
    caStatus clearChannelAction ( epicsGuard < casClientMutex > & );
    caStatus claimChannelAction ( epicsGuard < casClientMutex > & );
    caStatus writeNotifyAction ( epicsGuard < casClientMutex > & );
    caStatus clientNameAction ( epicsGuard < casClientMutex > & );
    caStatus hostNameAction ( epicsGuard < casClientMutex > & );
    caStatus searchAction ( epicsGuard < casClientMutex > & );
    caStatus sendErr ( epicsGuard < casClientMutex > &, 
        const caHdrLargeArray *curp, ca_uint32_t cid, 
        const int reportedStatus, const char * pformat, ... );
    caStatus readNotifyFailureResponse ( epicsGuard < casClientMutex > &,
        const caHdrLargeArray & msg, const caStatus ECA_XXXX );
    caStatus monitorFailureResponse ( epicsGuard < casClientMutex > &,
        const caHdrLargeArray & msg,  const caStatus ECA_XXXX );
	caStatus writeNotifyResponseECA_XXX ( epicsGuard < casClientMutex > &,
        const caHdrLargeArray & msg, const caStatus status );
    caStatus sendErrWithEpicsStatus ( epicsGuard < casClientMutex > &,
        const caHdrLargeArray * pMsg, ca_uint32_t cid, caStatus epicsStatus, 
        caStatus clientStatus );
    caStatus writeActionSendFailureStatus ( epicsGuard < casClientMutex > &, 
        const caHdrLargeArray &, ca_uint32_t cid, caStatus );

	//
	// one function for each CA request type that has
	// asynchronous completion
	//
    caStatus createChanResponse ( epicsGuard < casClientMutex > &,
        casCtx &, const pvAttachReturn & );
    caStatus readResponse ( epicsGuard < casClientMutex > &,
        casChannelI * pChan, const caHdrLargeArray & msg,
        const gdd & desc, const caStatus status );
    caStatus readNotifyResponse ( epicsGuard < casClientMutex > &,
        casChannelI *pChan, const caHdrLargeArray & msg,
	const gdd & desc, const caStatus status );
    caStatus writeResponse ( epicsGuard < casClientMutex > &, casChannelI &, 
        const caHdrLargeArray & msg, const caStatus status );
    caStatus writeNotifyResponse ( epicsGuard < casClientMutex > &, casChannelI &, 
        const caHdrLargeArray &, const caStatus status );
    caStatus monitorResponse ( epicsGuard < casClientMutex > &,
        casChannelI & chan, const caHdrLargeArray & msg, 
        const gdd & desc, const caStatus status );
    caStatus enumPostponedCreateChanResponse ( epicsGuard < casClientMutex > &,
        casChannelI & chan, const caHdrLargeArray & hdr );
    caStatus privateCreateChanResponse ( epicsGuard < casClientMutex > &,
        casChannelI & chan, const caHdrLargeArray & hdr, unsigned dbrType );
        caStatus channelCreateFailedResp ( epicsGuard < casClientMutex > &,
        const caHdrLargeArray &, const caStatus createStatus );
    caStatus channelDestroyEventNotify ( 
        epicsGuard < casClientMutex > & guard, 
        casChannelI * const pChan, ca_uint32_t sid );
        caStatus accessRightsResponse ( 
        casChannelI * pciu );
    caStatus accessRightsResponse ( 
        epicsGuard < casClientMutex > &, casChannelI * pciu );
    caStatus searchResponse ( 
        epicsGuard < casClientMutex > &, const caHdrLargeArray &, const pvExistReturn & );
    caStatus asyncSearchResponse ( 
        epicsGuard < casClientMutex > &, const caNetAddr & outAddr,
    	const caHdrLargeArray & msg, const pvExistReturn & retVal,
        ca_uint16_t protocolRevision, ca_uint32_t sequenceNumber );


    typedef caStatus ( casChannelI :: * PWriteMethod ) ( 
        const casCtx &, const gdd & );
	caStatus read ();
	caStatus write ( PWriteMethod );
	caStatus writeArrayData( PWriteMethod );
	caStatus writeScalarData( PWriteMethod );

    outBufClient::flushCondition xSend ( char * pBuf, bufSizeT nBytesToSend,
        bufSizeT & nBytesSent );
    inBufClient::fillCondition xRecv ( char * pBuf, bufSizeT nBytesToRecv,
        inBufClient::fillParameter parm, bufSizeT & nByesRecv );

    virtual xBlockingStatus blockingState () const = 0;

    virtual outBufClient::flushCondition osdSend ( const char *pBuf, bufSizeT nBytesReq,
        bufSizeT & nBytesActual ) = 0;
    virtual inBufClient::fillCondition osdRecv ( char *pBuf, bufSizeT nBytesReq,
        bufSizeT &nBytesActual ) = 0;
    virtual void forceDisconnect () = 0;
        caStatus casMonitorCallBack ( 
        epicsGuard < casClientMutex > &, casMonitor &, const gdd & );
    caStatus logBadIdWithFileAndLineno (    
        epicsGuard < casClientMutex > & guard, const caHdrLargeArray * mp,
        const void * dp, const int cacStatus, const char * pFileName, 
        const unsigned lineno, const unsigned idIn );
    void casChannelDestroyFromInterfaceNotify ( casChannelI & chan, 
        bool immediatedSestroyNeeded );
    static void issuePosponeWhenNonePendingWarning ( const char * pReqTypeStr );

    casStrmClient ( const casStrmClient & );
    casStrmClient & operator = ( const casStrmClient & );
};

#define logBadId(GUARD, MP, DP, CACSTAT, RESID) \
	this->logBadIdWithFileAndLineno ( GUARD, MP, DP, \
    CACSTAT, __FILE__, __LINE__, RESID )


inline ca_uint16_t casStrmClient::protocolRevision () const 
{
    return this->minor_version_number;
}

#endif // casStrmClienth
