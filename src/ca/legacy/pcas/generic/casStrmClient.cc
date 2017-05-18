/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *      Author  Jeffrey O. Hill
 */

// *must* be defined before including net_convert.h
typedef unsigned long arrayElementCount;

#include "osiWireFormat.h"
#include "net_convert.h"    // byte order conversion from libca
#include "dbMapper.h"       // ait to dbr types
#include "gddAppTable.h"    // EPICS application type table
#include "gddApps.h"        // gdd predefined application type codes
#include "errlog.h"
#include "osiPoolStatus.h"  // is there sufficent space in pool 

#define epicsExportSharedSymbols
#include "casStrmClient.h"
#include "casChannelI.h"
#include "casAsyncIOI.h"
#include "channelDestroyEvent.h"

#if defined(__BORLANDC__) && defined(__linux__)
namespace  std  {
const nothrow_t  nothrow ;
}
#endif

casStrmClient::pCASMsgHandler const casStrmClient::msgHandlers[] =
{
    & casStrmClient::versionAction,
    & casStrmClient::eventAddAction,
    & casStrmClient::eventCancelAction,
    & casStrmClient::readAction,
    & casStrmClient::writeAction,
    & casStrmClient::uknownMessageAction, 
    & casStrmClient::searchAction, 
    & casStrmClient::uknownMessageAction, 
    & casStrmClient::eventsOffAction,
    & casStrmClient::eventsOnAction,
    & casStrmClient::readSyncAction,
    & casStrmClient::uknownMessageAction,
    & casStrmClient::clearChannelAction,
    & casStrmClient::uknownMessageAction,
    & casStrmClient::uknownMessageAction,
    & casStrmClient::readNotifyAction,
    & casStrmClient::ignoreMsgAction,
    & casStrmClient::uknownMessageAction,
    & casStrmClient::claimChannelAction,
    & casStrmClient::writeNotifyAction,
    & casStrmClient::clientNameAction,
    & casStrmClient::hostNameAction,
    & casStrmClient::uknownMessageAction,
    & casStrmClient::echoAction,
    & casStrmClient::uknownMessageAction,
    & casStrmClient::uknownMessageAction,
    & casStrmClient::uknownMessageAction,
    & casStrmClient::uknownMessageAction
};

//
// casStrmClient::casStrmClient()
//
casStrmClient::casStrmClient ( 
        caServerI & cas, clientBufMemoryManager & mgrIn, 
        const caNetAddr & clientAddr ) :
    casCoreClient ( cas ),
    in ( *this, mgrIn, 1 ), 
    out ( *this, mgrIn ),
    _clientAddr ( clientAddr ),
    pUserName ( 0 ),
    pHostName ( 0 ),
    incommingBytesToDrain ( 0 ),
    pendingResponseStatus ( S_cas_success ),
    minor_version_number ( 0 ),
    reqPayloadNeedsByteSwap ( true ),
    responseIsPending ( false )
{
    this->pHostName = new char [1u];
    *this->pHostName = '\0';

    this->pUserName = new ( std::nothrow ) char [1u];
    if ( ! this->pUserName ) {
        delete [] this->pHostName;
        throw std::bad_alloc();
    }
    *this->pUserName= '\0';
}

//
// casStrmClient::~casStrmClient ()
//
casStrmClient::~casStrmClient ()
{
    while ( casChannelI * pChan = this->chanList.get() ) {
        pChan->uninstallFromPV ( this->eventSys );
        this->chanTable.remove ( *pChan );
        delete pChan;
    }
    delete [] this->pUserName;
    delete [] this->pHostName;
}

//
// casStrmClient :: processMsg ()
//
caStatus casStrmClient :: processMsg ()
{
    epicsGuard < casClientMutex > guard ( this->mutex );
    int status = S_cas_success;
    
    // protect against service returning s_casApp_success when it
    // returned S_casApp_postponeAsyncIO before, but no
    // asyn IO completed since the last attempt
    if ( this->isBlocked () ) {
        return S_casApp_postponeAsyncIO;
    }

    try {

        // drain message that does not fit
        if ( this->incommingBytesToDrain ) {
            unsigned bytesLeft = this->in.bytesPresent();
            if ( bytesLeft < this->incommingBytesToDrain ) {
                this->in.removeMsg ( bytesLeft );
                this->incommingBytesToDrain -= bytesLeft;
                return S_cas_success;
            }
            else {
                this->in.removeMsg ( this->incommingBytesToDrain );
                this->incommingBytesToDrain = 0u;
            }
        }

        //
        // process any messages in the in buffer
        //
        unsigned bytesLeft;
        while ( ( bytesLeft = this->in.bytesPresent() ) ) {
            caHdrLargeArray msgTmp;
            unsigned msgSize;
            ca_uint32_t hdrSize;
            char * rawMP;
            {
                //
                // copy as raw bytes in order to avoid
                // alignment problems
                //
                caHdr smallHdr;
                if ( bytesLeft < sizeof ( smallHdr ) ) {
                    status = S_cas_success;
                    break;
                }

                rawMP = this->in.msgPtr ();
                memcpy ( & smallHdr, rawMP, sizeof ( smallHdr ) );

                ca_uint32_t payloadSize = AlignedWireRef < epicsUInt16 > ( smallHdr.m_postsize );
                ca_uint32_t nElem = AlignedWireRef < epicsUInt16 > ( smallHdr.m_count );
                if ( payloadSize != 0xffff && nElem != 0xffff ) {
                    hdrSize = sizeof ( smallHdr );
                }
                else {
                    ca_uint32_t LWA[2];
                    hdrSize = sizeof ( smallHdr ) + sizeof ( LWA );
                    if ( bytesLeft < hdrSize ) {
                        status = S_cas_success;
                        break;
                    }
                    //
                    // copy as raw bytes in order to avoid
                    // alignment problems
                    //
                    memcpy ( LWA, rawMP + sizeof ( caHdr ), sizeof( LWA ) );
                    payloadSize = AlignedWireRef < epicsUInt32 > ( LWA[0] );
                    nElem = AlignedWireRef < epicsUInt32 > ( LWA[1] );
                }

                msgTmp.m_cmmd = AlignedWireRef < epicsUInt16 > ( smallHdr.m_cmmd );
                msgTmp.m_postsize = payloadSize;
                msgTmp.m_dataType = AlignedWireRef < epicsUInt16 > ( smallHdr.m_dataType );
                msgTmp.m_count = nElem;
                msgTmp.m_cid = AlignedWireRef < epicsUInt32 > ( smallHdr.m_cid );
                msgTmp.m_available = AlignedWireRef < epicsUInt32 > ( smallHdr.m_available );

                // disconnect clients that dont send 8 byte aligned payloads
                if ( payloadSize & 0x7 ) {
                    caServerI::dumpMsg ( this->pHostName, this->pUserName, & msgTmp, 0, 
                        "CAS: Stream request wasn't 8 byte aligned\n" );
                    this->sendErr ( guard, & msgTmp, invalidResID, ECA_INTERNAL, 
                        "Stream request wasn't 8 byte aligned" );
                    status = S_cas_internal;
                    break;
                }

                msgSize = hdrSize + payloadSize;
                if ( bytesLeft < msgSize ) {
                    status = S_cas_success;
                    if ( msgSize > this->in.bufferSize() ) {
                        this->in.expandBuffer (msgSize);
                        // msg to large - set up message drain
                        if ( msgSize > this->in.bufferSize() ) {
                            caServerI::dumpMsg ( this->pHostName, this->pUserName, & msgTmp, 0, 
                                "The client requested transfer is greater than available " 
                                "memory in server or EPICS_CA_MAX_ARRAY_BYTES\n" );
                            status = this->sendErr ( guard, & msgTmp, invalidResID, ECA_TOLARGE, 
                                "client's request didnt fit within the CA server's message buffer" );
                            if ( status == S_cas_success ) {
                                this->in.removeMsg ( bytesLeft );
                                this->incommingBytesToDrain = msgSize - bytesLeft;
                            }
                        }
                    }
                    break;
                }

                this->ctx.setMsg ( msgTmp, rawMP + hdrSize );

                if ( this->getCAS().getDebugLevel() > 2u ) {
                    caServerI::dumpMsg ( this->pHostName, this->pUserName, 
                        & msgTmp, rawMP + hdrSize, 0 );
                }
            }

            //
            // Reset the context to the default
            // (guarantees that previous message does not get mixed 
            // up with the current message)
            //
            this->ctx.setChannel ( NULL );
            this->ctx.setPV ( NULL );

            //
            // Call protocol stub
            //
            casStrmClient::pCASMsgHandler pHandler;
            if ( msgTmp.m_cmmd < NELEMENTS ( casStrmClient::msgHandlers ) ) {
                pHandler = this->casStrmClient::msgHandlers[msgTmp.m_cmmd];
            }
            else {
                pHandler = & casStrmClient::uknownMessageAction;
            }
            status = ( this->*pHandler ) ( guard );
            if ( status ) {
                break;
            }
            this->in.removeMsg ( msgSize );
            this->pendingResponseStatus = S_cas_success;
            this->reqPayloadNeedsByteSwap = true;
            this->responseIsPending = false;
            this->pValueRead.set ( 0 );
        }
    }
    catch ( std::bad_alloc & ) {
        this->sendErr ( guard,
            this->ctx.getMsg(), invalidResID, ECA_ALLOCMEM, 
            "inablility to allocate memory in "
            "the CA server - disconnected client" );
        status = S_cas_noMemory;
    }
    catch ( std::exception & except ) {
        this->sendErr ( guard,
            this->ctx.getMsg(), invalidResID, ECA_INTERNAL, 
            "C++ exception \"%s\" in server - "
            "disconnected client",
            except.what () );
        status = S_cas_internal;
    }
    catch ( ... ) {
        this->sendErr ( guard,
            this->ctx.getMsg(), invalidResID, ECA_INTERNAL, 
            "unexpected C++ exception in server "
            "disconnected client" );
        status = S_cas_internal;
    }

    return status;
}

//
// casStrmClient::uknownMessageAction()
//
caStatus casStrmClient::uknownMessageAction ( epicsGuard < casClientMutex > & guard )
{
    const caHdrLargeArray *mp = this->ctx.getMsg();
    caStatus status;

    caServerI::dumpMsg ( this->pHostName, 
        this->pUserName, mp, this->ctx.getData(),
        "bad request code from virtual circuit=%u\n", mp->m_cmmd );

    /* 
     *  most clients dont recover from this
     */
    status = this->sendErr ( guard, mp, invalidResID, 
        ECA_INTERNAL, "Invalid Request Code" );
    if (status) {
        return status;
    }

    /*
     * returning S_cas_badProtocol here disconnects
     * the client with the bad message
     */
    return S_cas_badProtocol;
}

/*
 * casStrmClient::ignoreMsgAction()
 */
caStatus casStrmClient::ignoreMsgAction ( epicsGuard < casClientMutex > & )
{
    return S_cas_success;
}

//
// versionAction()
//
caStatus casStrmClient::versionAction ( epicsGuard < casClientMutex > & )
{
#if 1
    return S_cas_success;
#else
    //
    // eventually need to set the priority here
    //
    const caHdrLargeArray * mp = this->ctx.getMsg();

    if ( mp->m_dataType > CA_PROTO_PRIORITY_MAX ) {
        return S_cas_badProtocol;
    }

    if (!CA_VSUPPORTED(mp->m_count)) {
        if ( this->getCAS().getDebugLevel() > 3u ) {
            char pHostName[64u];
            this->hostName ( pHostName, sizeof ( pHostName ) );
            printf ( "\"%s\" is too old\n",
                pHostName );
        }
        return S_cas_badProtocol;
    }

    double tmp = mp->m_dataType - CA_PROTO_PRIORITY_MIN;
    tmp *= epicsThreadPriorityCAServerHigh - epicsThreadPriorityCAServerLow;
    tmp /= CA_PROTO_PRIORITY_MAX - CA_PROTO_PRIORITY_MIN;
    tmp += epicsThreadPriorityCAServerLow;
    unsigned epicsPriorityNew = (unsigned) tmp;
    unsigned epicsPrioritySelf = epicsThreadGetPrioritySelf();
    if ( epicsPriorityNew != epicsPrioritySelf ) {
        epicsThreadBooleanStatus tbs;
        unsigned priorityOfEvents;
        tbs  = epicsThreadHighestPriorityLevelBelow ( epicsPriorityNew, &priorityOfEvents );
        if ( tbs != epicsThreadBooleanStatusSuccess ) {
            priorityOfEvents = epicsPriorityNew;
        }

        if ( epicsPriorityNew > epicsPrioritySelf ) {
            epicsThreadSetPriority ( epicsThreadGetIdSelf(), epicsPriorityNew );
            db_event_change_priority ( client->evuser, priorityOfEvents );
        }
        else {
            db_event_change_priority ( client->evuser, priorityOfEvents );
            epicsThreadSetPriority ( epicsThreadGetIdSelf(), epicsPriorityNew );
        }
        client->priority = mp->m_dataType;
    }
    return S_cas_success;
#endif
}

//
// echoAction()
//
caStatus casStrmClient::echoAction ( epicsGuard < casClientMutex > & )
{
    const caHdrLargeArray * mp = this->ctx.getMsg();
    const void * dp = this->ctx.getData();
    void * pPayloadOut;

    caStatus status = this->out.copyInHeader ( mp->m_cmmd, mp->m_postsize, 
        mp->m_dataType, mp->m_count, mp->m_cid, mp->m_available,
        & pPayloadOut );
    if ( ! status ) {
        memcpy ( pPayloadOut, dp, mp->m_postsize );
        this->out.commitMsg ();
    }
    return S_cas_success;
}

//
// casStrmClient::verifyRequest()
//
caStatus casStrmClient::verifyRequest (casChannelI * & pChan , bool allowdyn)
{
    //
    // channel exists for this resource id ?
    //
    chronIntId tmpId ( ctx.msg.m_cid );
    pChan = this->chanTable.lookup ( tmpId );
    if ( ! pChan ) {
        return ECA_BADCHID;
    }

    //
    // data type out of range ?
    //
    if ( ctx.msg.m_dataType > ((unsigned)LAST_BUFFER_TYPE) ) {
        return ECA_BADTYPE;
    }

    //
    // element count out of range ?
    //
    if ( ctx.msg.m_count > pChan->getMaxElem() ||
         ( !allowdyn && ctx.msg.m_count == 0u ) ) {
        return ECA_BADCOUNT;
    }

    this->ctx.setChannel ( pChan );
    this->ctx.setPV ( &pChan->getPVI() );

    return ECA_NORMAL;
}

void casStrmClient::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > locker ( this->mutex );
    printf ( "casStrmClient at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level > 1u ) {
        printf ("\tuser %s at %s\n", this->pUserName, this->pHostName);
        this->casCoreClient::show ( level - 1 );
        this->in.show ( level - 1 );
        this->out.show ( level - 1 );
        this->chanTable.show ( level - 1 );
    }
}

/*
 * casStrmClient::readAction()
 */
caStatus casStrmClient::readAction ( epicsGuard < casClientMutex > & guard )
{
    const caHdrLargeArray * mp = this->ctx.getMsg();
    casChannelI * pChan;

    {
        caStatus status = this->verifyRequest ( pChan, CA_V413 ( this->minor_version_number ) );
        if ( status != ECA_NORMAL ) {
            if ( pChan ) {
                return this->sendErr ( guard, mp, pChan->getCID(), 
                    status, "get request" );
            }
            else {
                return this->sendErr ( guard, mp, invalidResID, 
                    status, "get request" );
            }
        }
    }

    // dont allow a request that completed with the service in the
    // past, but was incomplete because no response was sent to be
    // executed twice with the service
    if ( this->responseIsPending ) {
        // dont read twice if we didnt finish in the past 
        // because we were send blocked
        if ( this->pendingResponseStatus == S_cas_success ) {
            assert ( pValueRead.valid () );
            return this->readResponse ( guard, pChan, *mp, 
                            *pValueRead, S_cas_success );
        }
        else {
            return this->sendErrWithEpicsStatus ( 
                                guard, mp, pChan->getCID(), 
                                this->pendingResponseStatus,
                                ECA_GETFAIL );
        }
    }

    /*
     * verify read access
     */
    if ( ! pChan->readAccess() ) {
        int v41 = CA_V41 ( this->minor_version_number );
        int cacStatus;
        if ( v41 ) {
            cacStatus = ECA_NORDACCESS;
        }
        else{
            cacStatus = ECA_GETFAIL;
        }
        return this->sendErr ( guard, mp, pChan->getCID(), 
            cacStatus, "read access denied" );
    }
    
    {
        caStatus servStat = this->read (); 
        if ( servStat == S_casApp_success ) {
            assert ( pValueRead.valid () );
            caStatus status = this->readResponse ( guard, pChan, *mp, 
                                *pValueRead, S_cas_success );
            this->responseIsPending = ( status != S_cas_success );
            return status;
        }
        else if ( servStat == S_casApp_asyncCompletion ) {
            return S_cas_success;
        }
        else if ( servStat == S_casApp_postponeAsyncIO ) {
            return S_casApp_postponeAsyncIO;
        }
        else {
            caStatus status =  this->sendErrWithEpicsStatus ( guard, mp, 
                pChan->getCID(), servStat, ECA_GETFAIL );
            if ( status != S_cas_success ) {
                this->pendingResponseStatus = servStat;
                this->responseIsPending = true;
            }
            return status;
        }
    }
}

//
// casStrmClient::readResponse()
//
caStatus casStrmClient::readResponse ( epicsGuard < casClientMutex > & guard,
                    casChannelI * pChan, const caHdrLargeArray & msg, 
                    const gdd & desc, const caStatus status )
{
    if ( status != S_casApp_success ) {
        return this->sendErrWithEpicsStatus ( guard, & msg, 
            pChan->getCID(), status, ECA_GETFAIL );
    }

    aitUint32 elementCount = 0;
    if (desc.isContainer()) {
        aitUint32 index;
        int gdds = gddApplicationTypeTable::app_table.mapAppToIndex
            ( desc.applicationType(), gddAppType_value, index );
        if ( gdds ) {
            return S_cas_badType;
        }
        elementCount = desc.getDD(index)->getDataSizeElements();
    } else {
        elementCount = desc.getDataSizeElements();
    }
    ca_uint32_t count = (msg.m_count == 0) ?
                            (ca_uint32_t)elementCount :
                             msg.m_count;

    void * pPayload;
    {
        unsigned payloadSize = dbr_size_n ( msg.m_dataType, count );
        caStatus localStatus = this->out.copyInHeader ( msg.m_cmmd, payloadSize,
            msg.m_dataType, count, pChan->getCID (),
            msg.m_available, & pPayload );
        if ( localStatus ) {
            if ( localStatus==S_cas_hugeRequest ) {
                localStatus = sendErr ( guard, & msg, pChan->getCID(), ECA_TOLARGE, 
                    "unable to fit read response into server's buffer" );
            }
            return localStatus;
        }
    }

    //
    // convert gdd to db_access type
    // (places the data in network format)
    //
    int mapDBRStatus = gddMapDbr[msg.m_dataType].conv_dbr(
        pPayload, count, desc, pChan->enumStringTable() );
    if ( mapDBRStatus < 0 ) {
        desc.dump ();
        errPrintf ( S_cas_badBounds, __FILE__, __LINE__, "- get with PV=%s type=%u count=%u",
                pChan->getPVI().getName(), msg.m_dataType, count );
        return this->sendErrWithEpicsStatus ( 
            guard, & msg, pChan->getCID(), S_cas_badBounds, ECA_GETFAIL );
    }
    int cacStatus = caNetConvert ( 
        msg.m_dataType, pPayload, pPayload, true, count );
    if ( cacStatus != ECA_NORMAL ) {
        return this->sendErrWithEpicsStatus ( 
            guard, & msg, pChan->getCID(), S_cas_internal, cacStatus );
    }
    if ( msg.m_dataType == DBR_STRING && count == 1u ) {
        unsigned reducedPayloadSize = strlen ( static_cast < char * > ( pPayload ) ) + 1u;
        this->out.commitMsg ( reducedPayloadSize );
    }
    else {
        this->out.commitMsg ();
    }

    return S_cas_success;
}

//
// casStrmClient::readNotifyAction()
//
caStatus casStrmClient::readNotifyAction ( epicsGuard < casClientMutex > & guard )
{
    const caHdrLargeArray * mp = this->ctx.getMsg();
    casChannelI * pChan;

    {
        caStatus status = this->verifyRequest ( pChan, CA_V413 ( this->minor_version_number ) );
        if ( status != ECA_NORMAL ) {
            return this->readNotifyFailureResponse ( guard, * mp, status );
        }
    }

    // dont allow a request that completed with the service in the
    // past, but was incomplete because no response was sent to be
    // executed twice with the service
    if ( this->responseIsPending ) {
        // dont read twice if we didnt finish in the past 
        // because we were send blocked
        if ( this->pendingResponseStatus == S_cas_success ) {
            assert ( pValueRead.valid () );
            return this->readNotifyResponse ( guard, pChan, *mp, 
                            *pValueRead, S_cas_success );
        }
        else {
            return this->readNotifyFailureResponse ( 
                                guard, *mp, ECA_GETFAIL );
        }
    }

    //
    // verify read access
    // 
    if ( ! pChan->readAccess() ) {
        return this->readNotifyFailureResponse ( guard, *mp, ECA_NORDACCESS );
    }

    {
        caStatus servStat = this->read (); 
        if ( servStat == S_casApp_success ) {
            assert ( pValueRead.valid () );
            caStatus status = this->readNotifyResponse ( 
                                    guard, pChan, *mp, 
                                    *pValueRead, servStat );
            this->responseIsPending = ( status != S_cas_success );
            return status;
        }
        else if ( servStat == S_casApp_asyncCompletion ) {
            return S_cas_success;
        }
        else if ( servStat == S_casApp_postponeAsyncIO ) {
            return S_casApp_postponeAsyncIO;
        }
        else {
            caStatus status = this->readNotifyFailureResponse ( 
                              guard, *mp, ECA_GETFAIL );
            if ( status != S_cas_success ) {
                this->pendingResponseStatus = servStat;
                this->responseIsPending = true;
            }
            return status;
        }
    }
}

//
// casStrmClient::readNotifyResponse()
//
caStatus casStrmClient::readNotifyResponse ( epicsGuard < casClientMutex > & guard, 
        casChannelI * pChan, const caHdrLargeArray & msg, const gdd & desc, 
        const caStatus completionStatus )
{
    if ( completionStatus != S_cas_success ) {
        caStatus ecaStatus =  this->readNotifyFailureResponse ( 
            guard, msg, ECA_GETFAIL );
        return ecaStatus;
    }

    aitUint32 elementCount = 0;
    if (desc.isContainer()) {
        aitUint32 index;
        int gdds = gddApplicationTypeTable::app_table.mapAppToIndex
            ( desc.applicationType(), gddAppType_value, index );
        if ( gdds ) {
            return S_cas_badType;
        }
        elementCount = desc.getDD(index)->getDataSizeElements();
    } else {
        elementCount = desc.getDataSizeElements();
    }
    ca_uint32_t count = (msg.m_count == 0) ?
                            (ca_uint32_t)elementCount :
                             msg.m_count;

    void *pPayload;
    {
        unsigned size = dbr_size_n ( msg.m_dataType, count );
        caStatus status = this->out.copyInHeader ( msg.m_cmmd, size,
                    msg.m_dataType, count, ECA_NORMAL,
                    msg.m_available, & pPayload );
        if ( status ) {
            if ( status == S_cas_hugeRequest ) {
                status = sendErr ( guard, & msg, pChan->getCID(), ECA_TOLARGE, 
                    "unable to fit read notify response into server's buffer" );
            }
            return status;
        }
    }

    //
    // convert gdd to db_access type
    //
    int mapDBRStatus = gddMapDbr[msg.m_dataType].conv_dbr ( pPayload, 
        count, desc, pChan->enumStringTable() );
    if ( mapDBRStatus < 0 ) {
        desc.dump();
        errPrintf ( S_cas_badBounds, __FILE__, __LINE__, 
            "- get notify with PV=%s type=%u count=%u",
            pChan->getPVI().getName(), msg.m_dataType, count );
        return this->readNotifyFailureResponse ( guard, msg, ECA_NOCONVERT );
    }

    int cacStatus = caNetConvert ( 
        msg.m_dataType, pPayload, pPayload, true, count );
    if ( cacStatus != ECA_NORMAL ) {
        return this->sendErrWithEpicsStatus ( 
            guard, & msg, pChan->getCID(), S_cas_internal, cacStatus );
    }

    if ( msg.m_dataType == DBR_STRING && count == 1u ) {
        unsigned reducedPayloadSize = strlen ( static_cast < char * > ( pPayload ) ) + 1u;
        this->out.commitMsg ( reducedPayloadSize );
    }
    else {
        this->out.commitMsg ();
    }

    return S_cas_success;
}

//
// casStrmClient::readNotifyFailureResponse ()
//
caStatus casStrmClient::readNotifyFailureResponse ( 
    epicsGuard < casClientMutex > &, const caHdrLargeArray & msg, const caStatus ECA_XXXX )
{
    assert ( ECA_XXXX != ECA_NORMAL );
    void *pPayload;
    unsigned size = dbr_size_n ( msg.m_dataType, msg.m_count );
    caStatus status = this->out.copyInHeader ( msg.m_cmmd, size,
                msg.m_dataType, msg.m_count, ECA_XXXX, 
                msg.m_available, & pPayload );
    if ( ! status ) {
        memset ( pPayload, '\0', size );
        this->out.commitMsg ();
    }
    return status;
}

//
// set bounds on an application type within a container, but dont 
// preallocate space (not preallocating buffer space allows gdd::put 
// to be more efficent if it discovers that the source has less data 
// than the destination)
//
caStatus convertContainerMemberToAtomic ( gdd & dd,
         aitUint32 appType, aitUint32 requestedCount, aitUint32 nativeCount )
{
    gdd * pVal;
    if ( dd.isContainer() ) {
        // All DBR types have a value member 
        aitUint32 index;
        int gdds = gddApplicationTypeTable::app_table.mapAppToIndex
            ( dd.applicationType(), appType, index );
        if ( gdds ) {
            return S_cas_badType;
        }

        pVal = dd.getDD ( index );
        if ( ! pVal ) {
            return S_cas_badType;
        }
    }
    else {
        pVal = & dd;
    }

    // we cant changed a managed type that is 
    // already atomic (array)
    if ( ! pVal->isScalar () ) {
        return S_cas_badType;
    }

    if ( nativeCount <= 1 ) {
        return S_cas_success;
    }
        
    // convert to atomic
    gddBounds bds;
    bds.setSize ( requestedCount );
    bds.setFirst ( 0u );
    pVal->setDimension ( 1u, & bds );
    return S_cas_success;
}

//
// createDBRDD ()
//
static caStatus createDBRDD ( unsigned dbrType,
        unsigned requestedCount, unsigned nativeCount, gdd * & pDD )
{
    /*
     * DBR type has already been checked, but it is possible
     * that "gddDbrToAit" will not track with changes in
     * the DBR_XXXX type system
     */
    if ( dbrType >= NELEMENTS ( gddDbrToAit ) ) {
        return S_cas_badType;
    }

    if ( gddDbrToAit[dbrType].type == aitEnumInvalid ) {
        return S_cas_badType;
    }

    aitUint16 appType = gddDbrToAit[dbrType].app;
    
    //
    // create the descriptor
    //
    gdd * pDescRet = 
        gddApplicationTypeTable::app_table.getDD ( appType );
    if ( ! pDescRet ) {
        return S_cas_noMemory;
    }

    // fix the value element count
    caStatus status = convertContainerMemberToAtomic ( 
        *pDescRet, gddAppType_value, requestedCount, nativeCount );
    if ( status != S_cas_success ) {
        pDescRet->unreference ();
        return status;
    }

    // fix the enum string table element count
    // (this is done here because the application type table in gdd 
    // does not appear to handle this correctly)
    if ( dbrType == DBR_CTRL_ENUM || dbrType == DBR_GR_ENUM ) {
        status = convertContainerMemberToAtomic ( 
            *pDescRet, gddAppType_enums, MAX_ENUM_STATES );
        if ( status != S_cas_success ) {
            pDescRet->unreference ();
            return status;
        }
    }

    pDD = pDescRet;
    return S_cas_success;
}

//
// casStrmClient::monitorFailureResponse ()
//
caStatus casStrmClient::monitorFailureResponse ( 
    epicsGuard < casClientMutex > &, const caHdrLargeArray & msg, 
    const caStatus ECA_XXXX )
{
    assert ( ECA_XXXX != ECA_NORMAL );
    void *pPayload;
    unsigned size = dbr_size_n ( msg.m_dataType, msg.m_count );
    caStatus status = this->out.copyInHeader ( msg.m_cmmd, size,
                msg.m_dataType, msg.m_count, ECA_XXXX, 
                msg.m_available, & pPayload );
    if ( ! status ) {
        memset ( pPayload, '\0', size );
        this->out.commitMsg ();
    }
    return status;
}

//
// casStrmClient::monitorResponse ()
//
caStatus casStrmClient::monitorResponse ( 
    epicsGuard < casClientMutex > & guard,
    casChannelI & chan, const caHdrLargeArray & msg, 
    const gdd & desc, const caStatus completionStatus )
{
    aitUint32 elementCount = 0;
    if (desc.isContainer()) {
        aitUint32 index;
        int gdds = gddApplicationTypeTable::app_table.mapAppToIndex
            ( desc.applicationType(), gddAppType_value, index );
        if ( gdds ) {
            return S_cas_badType;
        }
        elementCount = desc.getDD(index)->getDataSizeElements();
    } else {
        elementCount = desc.getDataSizeElements();
    }
    ca_uint32_t count = (msg.m_count == 0) ?
                            (ca_uint32_t)elementCount :
                             msg.m_count;

    void * pPayload = 0;
    {
        ca_uint32_t size = dbr_size_n ( msg.m_dataType, count );
        caStatus status = out.copyInHeader ( msg.m_cmmd, size,
            msg.m_dataType, count, ECA_NORMAL,
            msg.m_available, & pPayload );
        if ( status ) {
            if ( status == S_cas_hugeRequest ) {
                status = sendErr ( guard, & msg, chan.getCID(), ECA_TOLARGE, 
                    "unable to fit read subscription update response "
                    "into server's buffer" );
            }
            return status;
        }
    }

    if ( ! chan.readAccess () ) {
        return monitorFailureResponse ( guard, msg, ECA_NORDACCESS );
    }

    gdd * pDBRDD = 0;
    if ( completionStatus == S_cas_success ) {
        caStatus status = createDBRDD ( msg.m_dataType, count,
                chan.getMaxElem(), pDBRDD );
        if ( status != S_cas_success ) {
            caStatus ecaStatus;
            if ( status == S_cas_badType ) {
                ecaStatus = ECA_BADTYPE;
            }
            else if ( status == S_cas_noMemory ) {
                ecaStatus = ECA_ALLOCMEM;
            }
            else {
                ecaStatus = ECA_GETFAIL;
            }               
            return monitorFailureResponse ( guard, msg, ecaStatus );
        }
        else {
            gddStatus gdds = gddApplicationTypeTable::
                app_table.smartCopy ( pDBRDD, & desc );
            if ( gdds < 0 ) {
                pDBRDD->unreference ();
                errPrintf ( S_cas_noConvert, __FILE__, __LINE__,
        "no conversion between event app type=%d and DBR type=%d Element count=%d",
                    desc.applicationType (), msg.m_dataType, count);
                return monitorFailureResponse ( guard, msg, ECA_NOCONVERT );
            }
        }
    }
    else {
        if ( completionStatus == S_cas_noRead ) {
            return monitorFailureResponse ( guard, msg, ECA_NORDACCESS );
        }
        else if ( completionStatus == S_cas_noMemory || 
            completionStatus == S_casApp_noMemory ) {
            return monitorFailureResponse ( guard, msg, ECA_ALLOCMEM );
        }
        else if ( completionStatus == S_cas_badType ) {
            return monitorFailureResponse ( guard, msg, ECA_BADTYPE );
        }
        else {
            errMessage ( completionStatus, "- in monitor response" );
            return monitorFailureResponse ( guard, msg, ECA_GETFAIL );
        }
    }

    int mapDBRStatus = gddMapDbr[msg.m_dataType].conv_dbr ( 
        pPayload, count, *pDBRDD, chan.enumStringTable() );
    if ( mapDBRStatus < 0 ) {
        pDBRDD->unreference ();
        return monitorFailureResponse ( guard, msg, ECA_NOCONVERT );
    }

    int cacStatus = caNetConvert ( 
        msg.m_dataType, pPayload, pPayload, true, count );
    if ( cacStatus != ECA_NORMAL ) {
        pDBRDD->unreference ();
        return this->sendErrWithEpicsStatus ( 
            guard, & msg, chan.getCID(), S_cas_internal, cacStatus );
    }

    //
    // force string message size to be the true size 
    //
    if ( msg.m_dataType == DBR_STRING && count == 1u ) {
        ca_uint32_t reducedPayloadSize = strlen ( static_cast < char * > ( pPayload ) ) + 1u;
        this->out.commitMsg ( reducedPayloadSize );
    }
    else {
        this->out.commitMsg ();
    }

    pDBRDD->unreference ();

    return S_cas_success;
}

/*
 * casStrmClient::writeActionSendFailureStatus()
 */
caStatus casStrmClient ::
    writeActionSendFailureStatus ( epicsGuard < casClientMutex > & guard, 
        const caHdrLargeArray & msg, ca_uint32_t cid, caStatus status )
{   
    caStatus ecaStatus;
    if ( status == S_cas_noMemory ) {
        ecaStatus = ECA_ALLOCMEM;
    }
    else if ( status == S_cas_noConvert ) {
        ecaStatus = ECA_NOCONVERT;
    }
    else if ( status == S_cas_badType ) {
        ecaStatus = ECA_BADTYPE;
    }
    else {
        ecaStatus = ECA_PUTFAIL;
    }
    status = this->sendErrWithEpicsStatus ( guard, & msg, cid,
                status, ecaStatus );
    return status;
}


/*
 * casStrmClient::writeAction()
 */
caStatus casStrmClient::writeAction ( epicsGuard < casClientMutex > & guard )
{   
    const caHdrLargeArray *mp = this->ctx.getMsg();
    casChannelI *pChan;

    {
        caStatus status = this->verifyRequest ( pChan );
        if (status != ECA_NORMAL) {
            if ( pChan ) {
                return this->sendErr ( guard, mp, pChan->getCID(), 
                    status, "get request" );
            }
            else {
                return this->sendErr ( guard, mp, invalidResID, 
                    status, "get request" );
            }
        }
    }

    // dont allow a request that completed with the service in the
    // past, but was incomplete because no response was sent be
    // executed twice with the service
    if ( this->responseIsPending ) {
        caStatus status = this->writeActionSendFailureStatus ( 
                            guard, *mp, pChan->getCID(), 
                            this->pendingResponseStatus );
        return status;
    }

    //
    // verify write access
    // 
    if ( ! pChan->writeAccess() ) {
        caStatus status;
        int v41 = CA_V41 ( this->minor_version_number );
        if (v41) {
            status = ECA_NOWTACCESS;
        }
        else{
            status = ECA_PUTFAIL;
        }
        return this->sendErr ( guard, mp, pChan->getCID(),
            status, "write access denied");
    }

    //
    // initiate the  write operation
    //
    {
        caStatus servStat = this->write ( & casChannelI :: write ); 
        if ( servStat == S_casApp_success || 
                servStat == S_casApp_asyncCompletion ) {
            return S_cas_success;
        }
        else if ( servStat == S_casApp_postponeAsyncIO ) {
            return S_casApp_postponeAsyncIO;
        }
        else {
            caStatus status = 
                this->writeActionSendFailureStatus ( guard, *mp, 
                                        pChan->getCID(), servStat );
            if ( status != S_cas_success ) {
                this->pendingResponseStatus = servStat;
                this->responseIsPending = true;
            }
            return status;
        }
    }

    //
    // The gdd created above is deleted by the server tool 
    //
}

//
// casStrmClient::writeResponse()
//
caStatus casStrmClient::writeResponse (
    epicsGuard < casClientMutex > & guard, casChannelI & chan,
    const caHdrLargeArray & msg, const caStatus completionStatus )
{
    caStatus status;

    if ( completionStatus ) {
        errMessage ( completionStatus, "write failed" );
        status = this->sendErrWithEpicsStatus ( guard, & msg, 
                chan.getCID(), completionStatus, ECA_PUTFAIL );
    }
    else {
        status = S_cas_success;
    }

    return status;
}

/*
 * casStrmClient::writeNotifyAction()
 */
caStatus casStrmClient::writeNotifyAction ( 
    epicsGuard < casClientMutex > & guard )
{
    const caHdrLargeArray *mp = this->ctx.getMsg ();

    casChannelI *pChan;
    {
        caStatus status = this->verifyRequest ( pChan );
        if ( status != ECA_NORMAL ) {
            return casStrmClient::writeNotifyResponseECA_XXX ( guard, *mp, status );
        }
    }
    
    // dont allow a request that completed with the service in the
    // past, but was incomplete because no response was sent be
    // executed twice with the service
    if ( this->responseIsPending ) {
        caStatus status = this->writeNotifyResponse ( guard, *pChan, 
                    *mp, this->pendingResponseStatus );
        return status;
    }

    //
    // verify write access
    // 
    if ( ! pChan->writeAccess() ) {
        if ( CA_V41(this->minor_version_number) ) {
            return this->casStrmClient::writeNotifyResponseECA_XXX (
                    guard, *mp, ECA_NOWTACCESS);
        }
        else {
            return this->casStrmClient::writeNotifyResponse (
                    guard, *pChan, *mp, S_cas_noWrite );
        }
    }

    //
    // initiate the  write operation
    //
    {
        caStatus servStat = this->write ( & casChannelI :: writeNotify ); 
        if ( servStat == S_casApp_asyncCompletion ) {
            return S_cas_success;
        }
        else if ( servStat == S_casApp_postponeAsyncIO ) {
            return S_casApp_postponeAsyncIO;
        }
        else {
            caStatus status = this->writeNotifyResponse ( guard, *pChan, 
                                                        *mp, servStat );
            if ( status != S_cas_success ) {
                this->pendingResponseStatus = servStat;
                this->responseIsPending = true;
            }
            return status;
        }
    }
}

/* 
 * casStrmClient::writeNotifyResponse()
 */
caStatus casStrmClient::writeNotifyResponse ( epicsGuard < casClientMutex > & guard,
        casChannelI & chan, const caHdrLargeArray & msg, const caStatus completionStatus )
{
    caStatus ecaStatus;

    if ( completionStatus == S_cas_success ) {
        ecaStatus = ECA_NORMAL;
    }
    else {
        ecaStatus = ECA_PUTFAIL;    
    }

    ecaStatus = this->casStrmClient::writeNotifyResponseECA_XXX ( 
        guard, msg, ecaStatus );
    if (ecaStatus) {
        return ecaStatus;
    }

    //
    // send independent warning exception to the client so that they
    // will see the error string associated with this error code 
    // since the error string cant be sent with the put call back 
    // response (hopefully this is useful information)
    //
    // order is very important here because it determines that the put 
    // call back response is always sent, and that this warning exception
    // message will be sent at most one time. In rare instances it will
    // not be sent, but at least it will not be sent multiple times.
    // The message is logged to the console in the rare situations when
    // we are unable to send.
    //
    if ( completionStatus != S_cas_success ) {
        ecaStatus = this->sendErrWithEpicsStatus ( guard, & msg, chan.getCID(),
                        completionStatus, ECA_NOCONVERT );
        if ( ecaStatus ) {
            errMessage ( completionStatus, 
                "<= put callback failure detail not passed to client" );
        }
    }
    return S_cas_success;
}

/* 
 * casStrmClient::writeNotifyResponseECA_XXX()
 */
caStatus casStrmClient::writeNotifyResponseECA_XXX (
    epicsGuard < casClientMutex > &, 
    const caHdrLargeArray & msg, const caStatus ecaStatus )
{
    caStatus status = out.copyInHeader ( msg.m_cmmd, 0,
        msg.m_dataType, msg.m_count, ecaStatus, 
        msg.m_available, 0 );
    if ( ! status ) {
        this->out.commitMsg ();
    }

    return status;
}

//
// casStrmClient :: asyncSearchResp()
//
caStatus casStrmClient :: asyncSearchResponse ( 
    epicsGuard < casClientMutex > & guard, const caNetAddr & /* outAddr */,
    const caHdrLargeArray & msg, const pvExistReturn & retVal,
    ca_uint16_t /* protocolRevision */, ca_uint32_t /* sequenceNumber */ )
{
    return this->searchResponse ( guard, msg, retVal );
}

// casStrmClient :: hostName()
void casStrmClient :: hostName ( char * pInBuf, unsigned bufSizeIn ) const
{
    _clientAddr.stringConvert ( pInBuf, bufSizeIn );
}

//
// caStatus casStrmClient :: searchResponse()
//
caStatus casStrmClient :: searchResponse ( 
    epicsGuard < casClientMutex > & guard, 
    const caHdrLargeArray & msg, 
    const pvExistReturn & retVal )
{    
    if ( retVal.getStatus() != pverExistsHere ) {
        if (msg.m_dataType == DOREPLY ) {
            long status = this->out.copyInHeader ( CA_PROTO_NOT_FOUND, 0,
                msg.m_dataType, msg.m_count, msg.m_cid, msg.m_available, 0 );

            if ( status == S_cas_success ) {
                this->out.commitMsg ();
            }
        }
        return S_cas_success;
    }
    
    //
    // starting with V4.1 the count field is used (abused)
    // by the client to store the minor version number of 
    // the client.
    //
    // Old versions expect alloc of channel in response
    // to a search request. This is no longer supported.
    //
    if ( !CA_V44( msg.m_count ) ) {
        errlogPrintf ( 
            "client \"%s\" using EPICS R3.11 CA "
            "connect protocol was ignored\n", 
            pHostName );
        //
        // old connect protocol was dropped when the
        // new API was added to the server (they must
        // now use clients at EPICS 3.12 or higher)
        //
        caStatus status = this->sendErr ( 
            guard, & msg, invalidResID, ECA_DEFUNCT,
            "R3.11 connect sequence from old client was ignored" );
        return status;
    }

    //
    // cid field is abused to carry the IP 
    // address in CA_V48 or higher
    // (this allows a CA servers to serve
    // as a directory service)
    //
    // data type field is abused to carry the IP 
    // port number here CA_V44 or higher
    // (this allows multiple CA servers on one
    // host)
    //
    ca_uint32_t serverAddr;
    ca_uint16_t serverPort;
    if ( CA_V48( msg.m_count ) ) {    
        struct sockaddr_in  ina;
        if ( retVal.addrIsValid() ) {
            caNetAddr addr = retVal.getAddr();
            ina = addr.getSockIP();
            //
            // If they dont specify a port number then the default
            // CA server port is assumed when it it is a server
            // address redirect (it is never correct to use this
            // server's port when it is a redirect).
            //
            if ( ina.sin_port == 0u ) {
                ina.sin_port = htons ( CA_SERVER_PORT );
            }
        }
        else {
            //
            // We dont fill in the servers address here because
            // the client has a tcp circuit to us and he knows
            // our address
            //
            ina.sin_addr.s_addr = htonl ( ~0U );
            ina.sin_port = htons ( 0 );
        }
        serverAddr = ntohl ( ina.sin_addr.s_addr );
        serverPort = ntohs ( ina.sin_port );
    }
    else {
        serverAddr = ntohl ( ~0U );
        serverPort = ntohs ( 0 );
    }
    
    caStatus status = this->out.copyInHeader ( CA_PROTO_SEARCH, 
        0, serverPort, 0, serverAddr, msg.m_available, 0 );
    //
    // Starting with CA V4.1 the minor version number
    // is appended to the end of each search reply.
    // This value is ignored by earlier clients. 
    //
    if ( status == S_cas_success ) {
        this->out.commitMsg ();
    }
    
    return status;
}

//
// casStrmClient :: searchAction()
//
caStatus casStrmClient :: searchAction ( epicsGuard < casClientMutex > & guard )
{
    const caHdrLargeArray   *mp = this->ctx.getMsg();
    const char              *pChanName = static_cast <char * > ( this->ctx.getData() );
    caStatus                status;

    if (!CA_VSUPPORTED(mp->m_count)) {
        if ( this->getCAS().getDebugLevel() > 3u ) {
            char pHostName[64u];
            this->hostName ( pHostName, sizeof ( pHostName ) );
            printf ( "\"%s\" is searching for \"%s\" but is too old\n",
                pHostName, pChanName );
        }
        return S_cas_badProtocol;
    }

    //
    // check the sanity of the message
    //
    if ( mp->m_postsize <= 1 ) {
        caServerI::dumpMsg ( this->pHostName, "?", mp, this->ctx.getData(), 
            "empty PV name extension in TCP search request?\n" );
        return S_cas_success;
    }

    if ( pChanName[0] == '\0' ) {
        caServerI::dumpMsg ( this->pHostName, "?", mp, this->ctx.getData(), 
            "zero length PV name in UDP search request?\n" );
        return S_cas_success;
    }

    // check for an unterminated string before calling server tool
    // by searching backwards through the string (some early versions 
    // of the client library might not be setting the pad bytes to nill)
    for ( unsigned i = mp->m_postsize-1; pChanName[i] != '\0'; i-- ) {
        if ( i <= 1 ) {
            caServerI::dumpMsg ( pHostName, "?", mp, this->ctx.getData(), 
                "unterminated PV name in UDP search request?\n" );
            return S_cas_success;
        }
    }

    if ( this->getCAS().getDebugLevel() > 6u ) {
        this->hostName ( this->pHostName, sizeof ( pHostName ) );
        printf ( "\"%s\" is searching for \"%s\"\n", 
            pHostName, pChanName );
    }

    //
    // verify that we have sufficent memory for a PV and a
    // monitor prior to calling PV exist test so that when
    // the server runs out of memory we dont reply to
    // search requests, and therefore dont thrash through
    // caServer::pvExistTest() and casCreatePV::pvAttach()
    //
    if ( ! osiSufficentSpaceInPool ( 0 ) ) {
        return S_cas_success;
    }

    //
    // ask the server tool if this PV exists
    //
    this->userStartedAsyncIO = false;
    pvExistReturn pver = 
        this->getCAS()->pvExistTest ( 
            this->ctx, _clientAddr, pChanName );

    //
    // prevent problems when they initiate
    // async IO but dont return status
    // indicating so (and vise versa)
    //
    if ( this->userStartedAsyncIO ) {
        if ( pver.getStatus() != pverAsyncCompletion ) {
            errMessage ( S_cas_badParameter, 
                "- assuming asynch IO status from caServer::pvExistTest()");
        }
        status = S_cas_success;
    }
    else {
        //
        // otherwise we assume sync IO operation was initiated
        //
        switch ( pver.getStatus() ) {
        case pverExistsHere:
        case pverDoesNotExistHere:
            status = this->searchResponse ( guard, *mp, pver );
            break;

        case pverAsyncCompletion:
            errMessage ( S_cas_badParameter, 
                "- unexpected asynch IO status from "
                "caServer::pvExistTest() ignored");
            status = S_cas_success;
            break;

        default:
            errMessage ( S_cas_badParameter, 
                "- invalid return from "
                "caServer::pvExistTest() ignored");
            status = S_cas_success;
            break;
        }
    }
    return status;
}

/*
 * casStrmClient::hostNameAction()
 */
caStatus casStrmClient::hostNameAction ( epicsGuard < casClientMutex > & guard )
{
    const caHdrLargeArray *mp = this->ctx.getMsg();
    char            *pName = (char *) this->ctx.getData();
    unsigned        size;
    char            *pMalloc;
    caStatus        status;

    // currently this has to occur prior to 
    // creating channels or its not allowed
    if ( this->chanList.count () ) {
        return this->sendErr ( guard, mp, invalidResID, 
                        ECA_UNAVAILINSERV, pName );
    }

    size = strlen(pName)+1u;
    /*
     * user name will not change if there isnt enough memory
     */
    pMalloc = new char [size];
    if ( ! pMalloc ){
        status = this->sendErr ( guard, mp, invalidResID, 
                        ECA_ALLOCMEM, pName );
        if (status) {
            return status;
        }
        return S_cas_internal;
    }
    strncpy ( pMalloc, pName, size - 1 );
    pMalloc[ size - 1 ]='\0';

    if ( this->pHostName ) {
        delete [] this->pHostName;
    }
    this->pHostName = pMalloc;

    return S_cas_success;
}

/*
 * casStrmClient::clientNameAction()
 */
caStatus casStrmClient::clientNameAction ( 
    epicsGuard < casClientMutex > & guard )
{
    const caHdrLargeArray *mp = this->ctx.getMsg();
    char            *pName = (char *) this->ctx.getData();
    unsigned        size;
    char            *pMalloc;
    caStatus        status;

    // currently this has to occur prior to 
    // creating channels or its not allowed
    if ( this->chanList.count () ) {
        return this->sendErr ( guard, mp, invalidResID, 
                        ECA_UNAVAILINSERV, pName );
    }

    size = strlen(pName)+1;

    /*
     * user name will not change if there isnt enough memory
     */
    pMalloc = new char [size];
    if(!pMalloc){
        status = this->sendErr ( guard, mp, invalidResID, 
                    ECA_ALLOCMEM, pName );
        if (status) {
            return status;
        }
        return S_cas_internal;
    }
    strncpy ( pMalloc, pName, size - 1 );
    pMalloc[size-1]='\0';

    if ( this->pUserName ) {
        delete [] this->pUserName;
    }
    this->pUserName = pMalloc;

    return S_cas_success;
}

/*
 * casStrmClientMon::claimChannelAction()
 */
caStatus casStrmClient::claimChannelAction ( 
    epicsGuard < casClientMutex > & guard )
{
    const caHdrLargeArray * mp = this->ctx.getMsg();
    char *pName = (char *) this->ctx.getData();
    caServerI & cas = *this->ctx.getServer();

    /*
     * The available field is used (abused)
     * here to communicate the miner version number
     * starting with CA 4.1. The field was set to zero
     * prior to 4.1
     */
    if ( mp->m_available < 0xffff ) {
        this->minor_version_number = 
            static_cast < ca_uint16_t > ( mp->m_available );
    }
    else {
        this->minor_version_number = 0;
    }

    //
    // We shouldnt be receiving a connect message from 
    // an R3.11 client because we will not respond to their
    // search requests (if so we disconnect)
    //
    if ( ! CA_V44 ( this->minor_version_number ) ) {
        //
        // old connect protocol was dropped when the
        // new API was added to the server (they must
        // now use clients at EPICS 3.12 or higher)
        //
        caStatus status = this->sendErr ( guard, mp, mp->m_cid, ECA_DEFUNCT,
                "R3.11 connect sequence from old client was ignored");
        if ( status ) {
            return status;
        }
        return S_cas_badProtocol; // disconnect client
    }

    if ( mp->m_postsize <= 1u ) {
        return S_cas_badProtocol; // disconnect client
    }

    pName[mp->m_postsize-1u] = '\0';

    if ( ( mp->m_postsize - 1u ) > unreasonablePVNameSize ) {
        return S_cas_badProtocol; // disconnect client
    }

    this->userStartedAsyncIO = false;

    //
    // attach to the PV
    //
    pvAttachReturn pvar = cas->pvAttach ( this->ctx, pName );

    //
    // prevent problems when they initiate
    // async IO but dont return status
    // indicating so (and vise versa)
    //
    if ( this->userStartedAsyncIO ) {
        if ( pvar.getStatus() != S_casApp_asyncCompletion ) {
            fprintf ( stderr, 
                "Application returned %d from cas::pvAttach()"
                " - expected S_casApp_asyncCompletion\n",  
                pvar.getStatus() );
        }
        return S_cas_success;   
    }
    else if ( pvar.getStatus() == S_casApp_asyncCompletion ) {
        errMessage ( S_cas_badParameter, 
                    "- expected asynch IO creation "
                    "from caServer::pvAttach()" );
        return this->createChanResponse ( guard, 
                    this->ctx, S_cas_badParameter );
    }
    else if ( pvar.getStatus() == S_casApp_postponeAsyncIO ) {
        caServerI & casi ( * this->ctx.getServer() );
        if ( casi.ioIsPending () ) {
            casi.addItemToIOBLockedList ( *this );
            return S_casApp_postponeAsyncIO;
        }
        else {
            // Its not ok to postpone IO when there isnt at 
            // least one request pending. In that situation
            // there is no event from the service telling us 
            // when its ok to start issuing requests again!
            // So in that situation we tell the client that
            // the service refused the request, and this
            // caused the request to fail.
            this->issuePosponeWhenNonePendingWarning ( "PV attach channel" );
            return this->createChanResponse ( guard, this->ctx, 
                                    S_cas_posponeWhenNonePending );
        }
    }
    else {
        return this->createChanResponse ( guard, this->ctx, pvar );
    }
}

//
// casStrmClient::issuePosponeWhenNonePendingWarning()
//
void casStrmClient ::
    issuePosponeWhenNonePendingWarning ( const char * pReqTypeStr )
{
    errlogPrintf ( "service attempted to postpone %s IO when "
        "no IO was pending against the target\n", pReqTypeStr );
    errlogPrintf ( "server library will not receive a restart event, "
        "and so failure response was sent to client\n" );
}

//
// casStrmClient::createChanResponse()
//
caStatus casStrmClient::createChanResponse ( 
    epicsGuard < casClientMutex > & guard,
    casCtx & ctxIn, const pvAttachReturn & pvar )
{
    const caHdrLargeArray & hdr = *ctxIn.getMsg();

    if ( pvar.getStatus() != S_cas_success ) {
        return this->channelCreateFailedResp ( guard, 
            hdr, pvar.getStatus() );
    }

    if ( ! pvar.getPV()->pPVI ) {
        // @#$!* Tornado 2 Cygnus GNU compiler bugs
#       if ! defined (__GNUC__) || __GNUC__ > 2 || ( __GNUC__ == 2 && __GNUC_MINOR__ >= 92 )
            pvar.getPV()->pPVI = new ( std::nothrow )
                        casPVI ( *pvar.getPV() );
#       else
            try {
                pvar.getPV()->pPVI = new  
                            casPVI ( *pvar.getPV() );
            }
            catch ( ... ) {
                pvar.getPV()->pPVI = 0;
            }
#       endif

        if ( ! pvar.getPV()->pPVI ) {
            pvar.getPV()->destroyRequest ();
            return this->channelCreateFailedResp ( guard, hdr, S_casApp_pvNotFound );
        }
    }

    unsigned nativeTypeDBR;
    caStatus status = pvar.getPV()->pPVI->bestDBRType ( nativeTypeDBR );
    if ( status ) {
        pvar.getPV()->pPVI->deleteSignal();
        errMessage ( status, "best external dbr type fetch failed" );
        return this->channelCreateFailedResp ( guard, hdr, status );
    }

    //
    // attach the PV to this server
    //
    status = pvar.getPV()->pPVI->attachToServer ( this->getCAS() );
    if ( status ) {
        pvar.getPV()->pPVI->deleteSignal();
        return this->channelCreateFailedResp ( guard, hdr, status );
    }

    //
    // create server tool XXX derived from casChannel
    //
    casChannel * pChan = pvar.getPV()->pPVI->createChannel ( 
        ctxIn, this->pUserName, this->pHostName );
    if ( ! pChan ) {
        pvar.getPV()->pPVI->deleteSignal();
        return this->channelCreateFailedResp ( 
            guard, hdr, S_cas_noMemory );
    }

    if ( ! pChan->pChanI ) {
        // @#$!* Tornado 2 Cygnus GNU compiler bugs
#       if ! defined (__GNUC__) || __GNUC__ > 2 || ( __GNUC__ == 2 && __GNUC_MINOR__ >= 92 )
            pChan->pChanI = new ( std::nothrow )
                casChannelI ( * this, *pChan, 
                    * pvar.getPV()->pPVI, hdr.m_cid );
#       else
            try {
                pChan->pChanI = new 
                    casChannelI ( * this, *pChan, 
                        * pvar.getPV()->pPVI, hdr.m_cid );
            }
            catch ( ... ) {
                pChan->pChanI = 0;
            }
#       endif

        if ( ! pChan->pChanI ) {
            pChan->destroyRequest ();
            pChan->getPV()->pPVI->deleteSignal ();
            return this->channelCreateFailedResp ( 
                guard, hdr, S_cas_noMemory );
        }
    }

    //
    // Install the channel now so that the server will
    // clean up properly if the client disconnects
    // while an asynchronous IO fetching the enum
    // string table is outstanding
    //
    this->chanTable.idAssignAdd ( *pChan->pChanI );
    this->chanList.add ( *pChan->pChanI );
    pChan->pChanI->installIntoPV ();

    assert ( hdr.m_cid == pChan->pChanI->getCID() );

    //
    // check to see if the enum table is empty and therefore
    // an update is needed every time that a PV attaches 
    // to the server in case the client disconnected before 
    // an asynchronous IO to get the table completed
    //
    if ( nativeTypeDBR == DBR_ENUM ) {
        ctxIn.setChannel ( pChan->pChanI );
        ctxIn.setPV ( pvar.getPV()->pPVI );
        this->userStartedAsyncIO = false;
        status = pvar.getPV()->pPVI->updateEnumStringTable ( ctxIn );
        if ( this->userStartedAsyncIO ) {
            if ( status != S_casApp_asyncCompletion ) {
                fprintf ( stderr, 
                    "Application returned %d from casChannel::read()"
                    " - expected S_casApp_asyncCompletion\n", status);
            }
            status = S_cas_success;
        }
        else if ( status == S_cas_success ) {
            status = privateCreateChanResponse ( 
                guard, * pChan->pChanI, hdr, nativeTypeDBR );
        }
        else {
            if ( status == S_casApp_asyncCompletion )  {
                errMessage ( status, 
                    "- enum string tbl cache read returned asynch IO creation, but async IO not started?");
            }
            else if ( status == S_casApp_postponeAsyncIO ) {
                errMessage ( status, "- enum string tbl cache read ASYNC IO postponed ?");
                errlogPrintf ( "The server library does not currently support postponment of\n" );
                errlogPrintf ( "string table cache update of casChannel::read().\n" );
                errlogPrintf ( "To postpone this request please postpone the PC attach IO request.\n" );
                errlogPrintf ( "String table cache update did not occur.\n" );
            }
            else {
                errMessage ( status, "- enum string tbl cache read failed ?");
            }
            status = privateCreateChanResponse ( 
                guard, * pChan->pChanI, hdr, nativeTypeDBR );
        }
    }
    else {
        status = privateCreateChanResponse ( 
            guard, * pChan->pChanI, hdr, nativeTypeDBR );
    }
  
    if ( status != S_cas_success ) {
        this->chanTable.remove ( *pChan->pChanI );
        this->chanList.remove ( *pChan->pChanI );
        pChan->pChanI->uninstallFromPV ( this->eventSys );
        delete pChan->pChanI;
    }

    return status;
}

//
// casStrmClient::enumPostponedCreateChanResponse()
//
// LOCK must be applied
//
caStatus casStrmClient::enumPostponedCreateChanResponse ( 
    epicsGuard < casClientMutex > & guard, casChannelI & chan, 
    const caHdrLargeArray & hdr )
{
    caStatus status = this->privateCreateChanResponse ( 
        guard, chan, hdr, DBR_ENUM );
    if ( status != S_cas_success ) {
        if ( status != S_cas_sendBlocked ) {
            this->chanTable.remove ( chan );
            this->chanList.remove ( chan );
            chan.uninstallFromPV ( this->eventSys );
            delete & chan;
        }
    }
    return status;
}

//
// privateCreateChanResponse
//
caStatus casStrmClient::privateCreateChanResponse ( 
    epicsGuard < casClientMutex > & guard,
    casChannelI & chan, const caHdrLargeArray & hdr,
    unsigned nativeTypeDBR )
{
    //
    // We are allocating enough space for both the claim
    // response and the access rights response so that we know for
    // certain that they will both be sent together.
    //
    // Considering the possibility of large arrays we must allocate
    // an additional 2 * sizeof(ca_uint32_t)
    //
    void *pRaw;
    const outBufCtx outctx = this->out.pushCtx 
                    ( 0, 2 * sizeof ( caHdr ) + 2 * sizeof(ca_uint32_t), pRaw );
    if ( outctx.pushResult() != outBufCtx::pushCtxSuccess ) {
        return S_cas_sendBlocked;
    }

    //
    // We are certain that the request will complete
    // here because we allocated enough space for this
    // and the claim response above.
    //
    caStatus status = this->accessRightsResponse ( guard, & chan );
    if ( status ) {
        this->out.popCtx ( outctx );
        errMessage ( status, "incomplete channel create?" );
        status = this->channelCreateFailedResp ( guard, hdr, status );
        if ( status != S_cas_sendBlocked ) {
            this->chanTable.remove ( chan );
            this->chanList.remove ( chan );
            chan.uninstallFromPV ( this->eventSys );
            delete & chan;
        }
        return status;
    }

    //
    // We are allocated enough space for both the claim
    // response and the access response so that we know for
    // certain that they will both be sent together.
    // Nevertheles, some (old) clients do not receive
    // an access rights response so we allocate again
    // here to be certain that we are at the correct place in
    // the protocol buffer.
    //
    assert ( nativeTypeDBR <= 0xffff );
    aitIndex nativeCount = chan.getMaxElem();
    assert ( nativeCount <= 0xffffffff );
    assert ( hdr.m_cid == chan.getCID() );
    status = this->out.copyInHeader ( CA_PROTO_CREATE_CHAN, 0,
        static_cast <ca_uint16_t> ( nativeTypeDBR ), 
        static_cast <ca_uint32_t> ( nativeCount ),
        chan.getCID(), chan.getSID(), 0 );
    if ( status != S_cas_success ) {
        this->out.popCtx ( outctx );
        errMessage ( status, "incomplete channel create?" );
        status = this->channelCreateFailedResp ( guard, hdr, status );
        if ( status != S_cas_sendBlocked ) {
            this->chanTable.remove ( chan );
            this->chanList.remove ( chan );
            chan.uninstallFromPV ( this->eventSys );
            delete & chan;
        }
        return status;
    }

    this->out.commitMsg ();

    //
    // commit the message
    //
    bufSizeT nBytes = this->out.popCtx ( outctx );
    assert ( 
        nBytes == 2 * sizeof ( caHdr ) || 
        nBytes == 2 * sizeof ( caHdr ) + 2 * sizeof ( ca_uint32_t ) );
    this->out.commitRawMsg ( nBytes );

    return status;
}

/*
 * casStrmClient::channelCreateFailed()
 */
caStatus casStrmClient::channelCreateFailedResp ( 
    epicsGuard < casClientMutex > & guard, const caHdrLargeArray & hdr, 
    const caStatus createStatus )
{
    if ( createStatus == S_casApp_asyncCompletion ) {
        errMessage( S_cas_badParameter, 
            "- no asynchronous IO create in pvAttach() ?");
        errMessage( S_cas_badParameter, 
            "- or S_casApp_asyncCompletion was "
            "async IO competion code ?");
    }
    else if ( createStatus != S_casApp_pvNotFound ) {
        errMessage ( createStatus, 
            "- Server unable to create a new PV" );
    }
    caStatus status;
    if ( CA_V46 ( this->minor_version_number ) ) {
        status = this->out.copyInHeader ( 
            CA_PROTO_CREATE_CH_FAIL, 0,
            0, 0, hdr.m_cid, 0, 0 );
        if ( status == S_cas_success ) {
            this->out.commitMsg ();
        }
    }
    else {
        status = this->sendErrWithEpicsStatus ( 
            guard, & hdr, hdr.m_cid, createStatus, ECA_ALLOCMEM );
    }
    return status;
}

//
// casStrmClient::eventsOnAction()
//
caStatus casStrmClient::eventsOnAction ( epicsGuard < casClientMutex > & )
{
    this->enableEvents ();
    return S_cas_success;
}

//
// casStrmClient::eventsOffAction()
//
caStatus casStrmClient::eventsOffAction ( epicsGuard < casClientMutex > & )
{
    this->disableEvents ();
    return S_cas_success;
}

//
// eventAddAction()
//
caStatus casStrmClient::eventAddAction ( 
    epicsGuard < casClientMutex > & guard )
{
    const caHdrLargeArray *mp = this->ctx.getMsg();
    struct mon_info *pMonInfo = (struct mon_info *) 
                    this->ctx.getData();

    casChannelI *pciu;
    {
        caStatus status = casStrmClient::verifyRequest ( pciu, CA_V413 ( this->minor_version_number ) );
        if ( status != ECA_NORMAL ) {
            if ( pciu ) {
                return this->sendErr ( guard, mp, 
                    pciu->getCID(), status, NULL);
            }
            else {
                return this->sendErr ( guard, mp, 
                    invalidResID, status, NULL );
            }
        }
    }

    // dont allow a request that completed with the service in the
    // past, but was incomplete because no response was sent to be
    // executed twice with the service
    if ( this->responseIsPending ) {
        // dont read twice if we didnt finish in the past 
        // because we were send blocked
        if ( this->pendingResponseStatus == S_cas_success ) {
            assert ( pValueRead.valid () );
            return this->monitorResponse ( guard, *pciu, 
                                    *mp, *pValueRead, S_cas_success ); 
        }
        else {
            return this->monitorFailureResponse ( 
                                    guard, *mp, ECA_GETFAIL );
        }
    }

    //
    // place monitor mask in correct byte order
    //
    casEventMask mask;
    ca_uint16_t caProtoMask = AlignedWireRef < epicsUInt16 > ( pMonInfo->m_mask );
    if (caProtoMask&DBE_VALUE) {
        mask |= this->getCAS().valueEventMask();
    }

    if (caProtoMask&DBE_LOG) {
        mask |= this->getCAS().logEventMask();
    }
    
    if (caProtoMask&DBE_ALARM) {
        mask |= this->getCAS().alarmEventMask();
    }
    if (caProtoMask&DBE_PROPERTY) {
        mask |= this->getCAS().propertyEventMask();
    }

    if (mask.noEventsSelected()) {
        char errStr[40];
        sprintf ( errStr, "event add req with mask=0X%X\n", caProtoMask );
        return this->sendErr ( guard, mp, pciu->getCID(), 
            ECA_BADMASK, errStr );
    }

    casMonitor & mon = this->monitorFactory (
                *pciu, mp->m_available, mp->m_count, 
                mp->m_dataType, mask );
    pciu->installMonitor ( mon );


    //
    // Attempt to read the first monitored value prior to creating
    // the monitor object so that if the server tool chooses
    // to postpone asynchronous IO we can safely restart this
    // request later.
    //
    {
        caStatus servStat = this->read (); 
        //
        // always send immediate monitor response at event add
        //
        if ( servStat == S_cas_success ) {
            assert ( pValueRead.valid () );
            caStatus status = this->monitorResponse ( guard, *pciu, 
                                    *mp, *pValueRead, servStat ); 
            this->responseIsPending = ( status != S_cas_success );
            return status;
        }
        else if ( servStat == S_casApp_asyncCompletion ) {
            return S_cas_success;
        }
        else if ( servStat == S_casApp_postponeAsyncIO ) {
            return S_casApp_postponeAsyncIO;
        }
        else {
            caStatus status = this->monitorFailureResponse ( 
                                    guard, *mp, ECA_GETFAIL );
            if ( status != S_cas_success ) {
                pendingResponseStatus = servStat;
                this->responseIsPending = true;
            }
            return status;
        }
    }
}


//
// casStrmClient::clearChannelAction()
//
caStatus casStrmClient::clearChannelAction ( 
    epicsGuard < casClientMutex > & guard )
{
    const caHdrLargeArray * mp = this->ctx.getMsg ();
    const void * dp = this->ctx.getData ();
    int status;

    // send delete confirmed message
    status = this->out.copyInHeader ( mp->m_cmmd, 0,
        mp->m_dataType, mp->m_count, 
        mp->m_cid, mp->m_available, 0 );
    if ( status ) {
        return status;
    }
    this->out.commitMsg ();

    // Verify the channel
    chronIntId tmpId ( mp->m_cid );
    casChannelI * pciu = this->chanTable.remove ( tmpId );
    if ( pciu ) {
        this->chanList.remove ( *pciu );
        pciu->uninstallFromPV ( this->eventSys );
        delete pciu;
    }
    else {
        /*
         * it is possible that the channel delete arrives just 
         * after the server tool has deleted the PV so we will
         * not disconnect the client in this case. Nevertheless,
         * we send a warning message in case either the client 
         * or server has become corrupted
         */
        logBadId ( guard, mp, dp, ECA_BADCHID, mp->m_cid );
    }

    return status;
}

//
// If the channel pointer is nill this indicates that 
// the existence of the channel isnt certain because 
// it is still installed and the client or the server 
// tool might have destroyed it. Therefore, we must 
// locate it using the supplied server id.
//
// If the channel pointer isnt nill this indicates 
// that the channel has already been uninstalled.
//
// In both situations we need to send a channel 
// disconnect message to the client and destroy the 
// channel.
//
caStatus casStrmClient::channelDestroyEventNotify ( 
    epicsGuard < casClientMutex > &, 
    casChannelI * const pChan, ca_uint32_t sid )
{
    casChannelI * pChanFound;
    if ( pChan ) {
        pChanFound = pChan;
    }
    else {
        chronIntId tmpId ( sid );
        pChanFound = 
            this->chanTable.lookup ( tmpId );
        if ( ! pChanFound ) {
            return S_cas_success;
        }
    }

    if ( CA_V47 ( this->minor_version_number ) ) {
        caStatus status = this->out.copyInHeader ( 
            CA_PROTO_SERVER_DISCONN, 0,
            0, 0, pChanFound->getCID(), 0, 0 );
        if ( status == S_cas_sendBlocked ) {
            return status;
        }
        this->out.commitMsg ();
    }
    else {
        this->forceDisconnect ();
    }

    if ( ! pChan ) {
        this->chanTable.remove ( * pChanFound );
        this->chanList.remove ( * pChanFound );
        pChanFound->uninstallFromPV ( this->eventSys );
    }

    delete pChanFound;

    return S_cas_success;
}

// casStrmClient::casChannelDestroyFromInterfaceNotify()
// immediateUninstallNeeded is false when we must avoid 
// taking the lock in situations where we would compromise 
// the lock hierarchy
void casStrmClient::casChannelDestroyFromInterfaceNotify ( 
    casChannelI & chan, bool immediateUninstallNeeded )
{
    if ( immediateUninstallNeeded ) {
        epicsGuard < casClientMutex > guard ( this->mutex );

        this->chanTable.remove ( chan );
        this->chanList.remove ( chan );
        chan.uninstallFromPV ( this->eventSys );
    }

    class channelDestroyEvent * pEvent = 
        new ( std::nothrow ) class channelDestroyEvent (
            immediateUninstallNeeded ? & chan : 0,
            chan.getSID() );
    if ( pEvent ) {
        this->addToEventQueue ( *pEvent );
    }
    else {
        this->forceDisconnect ();
        if ( immediateUninstallNeeded ) {
            delete & chan;
        }
    }
}

// casStrmClient::eventCancelAction()
caStatus casStrmClient::eventCancelAction ( 
    epicsGuard < casClientMutex > & guard )
{
    const caHdrLargeArray * mp = this->ctx.getMsg ();
    const void * dp = this->ctx.getData ();

    {
        chronIntId tmpId ( mp->m_cid );
        casChannelI * pChan = this->chanTable.lookup ( tmpId );
        if ( ! pChan ) {
            // It is possible that the event delete arrives just 
            // after the server tool has deleted the PV. Its probably 
            // best to just diconnect for now since some old clients
            // may still exist.
            logBadId ( guard, mp, dp, ECA_BADCHID, mp->m_cid );
            return S_cas_badResourceId;
        }

        caStatus status = this->out.copyInHeader ( 
            CA_PROTO_EVENT_ADD, 0,
            mp->m_dataType, mp->m_count, 
            mp->m_cid, mp->m_available, 0 );
        if ( status != S_cas_success ) {
            return status;
        }
        this->out.commitMsg ();

        casMonitor * pMon = pChan->removeMonitor ( mp->m_available );
        if ( pMon ) {
            this->eventSys.prepareMonitorForDestroy ( *pMon );
        }
        else {
            // this indicates client or server library 
            // corruption so a disconnect is probably
            // the best option
            logBadId ( guard, mp, dp, ECA_BADMONID, mp->m_available );
            return S_cas_badResourceId;
        }
    }

    return S_cas_success;
}

#if 0
/*
 * casStrmClient::noReadAccessEvent()
 *
 * substantial complication introduced here by the need for backwards
 * compatibility
 */
caStatus casStrmClient::noReadAccessEvent ( 
    epicsGuard < casClientMutex > & guard, casClientMon * pMon )
{
    caHdr falseReply;
    unsigned size;
    caHdr * reply;
    int status;

    size = dbr_size_n ( pMon->getType(), pMon->getCount() );

    falseReply.m_cmmd = CA_PROTO_EVENT_ADD;
    falseReply.m_postsize = size;
    falseReply.m_dataType = pMon->getType();
    falseReply.m_count = pMon->getCount();
    falseReply.m_cid = pMon->getChannel().getCID();
    falseReply.m_available = pMon->getClientId();

    status = this->allocMsg ( size, &reply );
    if ( status ) {
        if( status == S_cas_hugeRequest ) {
            return this->sendErr ( &falseReply, ECA_TOLARGE, NULL );
        }
        return status;
    }
    else{
        /*
         * New clients recv the status of the
         * operation directly to the
         * event/put/get callback.
         *
         * Fetched value is zerod in case they
         * use it even when the status indicates 
         * failure.
         *
         * The m_cid field in the protocol
         * header is abused to carry the status
         */
        *reply = falseReply;
        reply->m_postsize = size;
        reply->m_cid = ECA_NORDACCESS;
        memset((char *)(reply+1), 0, size);
        this->commitMsg ();
    }
    
    return S_cas_success;
}
#endif

//
// casStrmClient::readSyncAction()
//
// This message indicates that the R3.13 or before client
// timed out on a read so we must clear out any pending 
// asynchronous IO associated with a read.
//
caStatus casStrmClient::readSyncAction ( epicsGuard < casClientMutex > & )
{
    tsDLIter < casChannelI > iter = 
                this->chanList.firstIter ();
    while ( iter.valid() ) {
        iter->clearOutstandingReads ();
        iter++;
    }

    const caHdrLargeArray * mp = this->ctx.getMsg ();

    int status = this->out.copyInHeader ( mp->m_cmmd, 0,
        mp->m_dataType, mp->m_count, 
        mp->m_cid, mp->m_available, 0 );
    if ( ! status ) {
        this->out.commitMsg ();
    }

    return status;
}

 //
 // casStrmClient::accessRightsResponse()
 //
 // NOTE:
 // Do not change the size of this response without making
 // parallel changes in createChanResp
 //
caStatus casStrmClient::accessRightsResponse ( casChannelI * pciu )
{
    epicsGuard < casClientMutex > guard ( this->mutex );
    return this->accessRightsResponse ( guard, pciu );
}

caStatus casStrmClient::accessRightsResponse ( 
    epicsGuard < casClientMutex > &, casChannelI * pciu )
{
    unsigned ar;
    int v41;
    int status;
    
    // noop if this is an old client
    v41 = CA_V41 ( this->minor_version_number );
    if ( ! v41 ) {
        return S_cas_success;
    }
    
    ar = 0; // none 
    if ( pciu->readAccess() ) {
        ar |= CA_PROTO_ACCESS_RIGHT_READ;
    }
    if ( pciu->writeAccess() ) {
        ar |= CA_PROTO_ACCESS_RIGHT_WRITE;
    }
    
    status = this->out.copyInHeader ( CA_PROTO_ACCESS_RIGHTS, 0,
        0, 0, pciu->getCID(), ar, 0 );
    if ( ! status ) {
        this->out.commitMsg ();
    }
    
    return status;
}

//
// casStrmClient::write()
//
caStatus casStrmClient :: write ( PWriteMethod pWriteMethod )
{
    const caHdrLargeArray *pHdr = this->ctx.getMsg();

    // no puts via compound types (for now)
    if (dbr_value_offset[pHdr->m_dataType]) {
        return S_cas_badType;
    }

    // dont byte swap twice 
    if ( this->reqPayloadNeedsByteSwap ) {
        int cacStatus = caNetConvert ( 
            pHdr->m_dataType, this->ctx.getData(), this->ctx.getData(), 
            false, pHdr->m_count );
        if ( cacStatus != ECA_NORMAL ) {
            return S_cas_badType;
        }
        this->reqPayloadNeedsByteSwap = false;
    }

    //
    // clear async IO flag
    //
    this->userStartedAsyncIO = false;

    //
    // DBR_STRING is stored outside the DD so it
    // lumped in with arrays
    //
    {
        caStatus servStatus;
        if ( pHdr->m_count > 1u ) {
            servStatus = this->writeArrayData ( pWriteMethod );
        }
        else {
            servStatus = this->writeScalarData ( pWriteMethod );
        }

        //
        // prevent problems when they initiate
        // async IO but dont return status
        // indicating so (and vise versa)
        //
        if ( this->userStartedAsyncIO ) {
            if ( servStatus != S_casApp_asyncCompletion ) {
                errlogPrintf ( 
                    "Application returned %d from casChannel::write() - "
                    "expected S_casApp_asyncCompletion\n",
                    servStatus );
                servStatus = S_casApp_asyncCompletion;
            }
        }
        else if ( servStatus == S_casApp_postponeAsyncIO ) {
            casPVI & pvi ( this->ctx.getChannel()->getPVI() );
            if ( pvi.ioIsPending () ) {
                pvi.addItemToIOBLockedList ( *this );
            }
            else {
                // Its not ok to postpone IO when there isnt at 
                // least one request pending. In that situation
                // there is no event from the service telling us 
                // when its ok to start issuing requests again!
                // So in that situation we tell the client that
                // the service refused the request, and this
                // caused the request to fail.
                this->issuePosponeWhenNonePendingWarning ( "write" );
                servStatus = S_cas_posponeWhenNonePending;
            }
        }
        else {
            if ( servStatus == S_casApp_asyncCompletion ) {
                servStatus = S_cas_badParameter;
                errMessage ( servStatus, 
                    "- expected asynch IO creation from casChannel::write()" );
            }
        }

        return servStatus;
    }
}

//
// casStrmClient::writeScalarData()
//
caStatus casStrmClient :: writeScalarData ( PWriteMethod pWriteMethod )
{
    const caHdrLargeArray * pHdr = this->ctx.getMsg ();

    /*
     * DBR type has already been checked, but it is possible
     * that "gddDbrToAit" will not track with changes in
     * the DBR_XXXX type system
     */
    if ( pHdr->m_dataType >= NELEMENTS(gddDbrToAit) ) {
        return S_cas_badType;
    }

    // a primitive type matching the atomic DBR_XXX type
    aitEnum type = gddDbrToAit[pHdr->m_dataType].type;
    if ( type == aitEnumInvalid ) {
        return S_cas_badType;
    }

    // the application type best maching this DBR_XXX type
    aitUint16 app = gddDbrToAit[pHdr->m_dataType].app;

    // When possible, preconvert to best external type in order
    // to reduce problems in the services
    aitEnum bestWritePrimType = 
        app == gddAppType_value ?
        this->ctx.getPV()->bestExternalType () :
        type;

    gdd * pDD = new gddScalar ( app, bestWritePrimType );
    if ( ! pDD ) {
        return S_cas_noMemory;
    }

    //
    // copy in, and convert to native type, the incoming data
    //
    gddStatus gddStat = aitConvert ( 
        pDD->primitiveType(), pDD->dataVoid(), type, 
        this->ctx.getData(), 1, &this->ctx.getPV()->enumStringTable() );
    caStatus status = S_cas_noConvert;
    if ( gddStat >= 0 ) { 
        //
        // set the status and severity to normal
        //
        pDD->setStat ( epicsAlarmNone );
        pDD->setSevr ( epicsSevNone );

        //
        // set the time stamp to the last time that
        // we added bytes to the in buf
        //
        aitTimeStamp gddts = this->lastRecvTS;
        pDD->setTimeStamp ( & gddts );

        //
        // call the server tool's virtual function
        //
        status = ( this->ctx.getChannel()->*pWriteMethod ) ( this->ctx, *pDD );
    }

    //
    // reference count is managed by smart pointer class
    // from here down
    //
    gddStat = pDD->unreference();
    assert ( ! gddStat );

    return status;
}

//
// casStrmClient::writeArrayData()
//
caStatus casStrmClient :: writeArrayData ( PWriteMethod pWriteMethod )
{
    const caHdrLargeArray *pHdr = this->ctx.getMsg ();

    /*
     * DBR type has already been checked, but it is possible
     * that "gddDbrToAit" will not track with changes in
     * the DBR_XXXX type system
     */
    if ( pHdr->m_dataType >= NELEMENTS(gddDbrToAit) ) {
        return S_cas_badType;
    }
    aitEnum type = gddDbrToAit[pHdr->m_dataType].type;
    if ( type == aitEnumInvalid ) {
        return S_cas_badType;
    }

    aitEnum bestExternalType = this->ctx.getPV()->bestExternalType ();

    // the application type best maching this DBR_XXX type
    aitUint16 app = gddDbrToAit[pHdr->m_dataType].app;

    // When possible, preconvert to best external type in order
    // to reduce problems in the services
    aitEnum bestWritePrimType = 
        app == gddAppType_value ?
        this->ctx.getPV()->bestExternalType () :
        type;

    gdd * pDD = new gddAtomic( app, bestWritePrimType, 1, pHdr->m_count);
    if ( ! pDD ) {
        return S_cas_noMemory;
    }

    size_t size = aitSize[bestExternalType] * pHdr->m_count;
    char * pData = new char [size];
    if ( ! pData ) {
        pDD->unreference();
        return S_cas_noMemory;
    }

    //
    // ok to use the default gddDestructor here because
    // an array of characters was allocated above
    //
    gddDestructor * pDestructor = new gddDestructor;
    if ( ! pDestructor ) {
        pDD->unreference();
        delete [] pData;
        return S_cas_noMemory;
    }

    //
    // install allocated area into the DD
    //
    pDD->putRef ( pData, bestWritePrimType, pDestructor );

    //
    // convert the data from the protocol buffer
    // to the allocated area so that they
    // will be allowed to ref the DD
    //
    caStatus status = S_cas_noConvert;
    gddStatus gddStat = aitConvert ( bestWritePrimType, 
        pData, type, this->ctx.getData(), 
        pHdr->m_count, &this->ctx.getPV()->enumStringTable() );
    if ( gddStat >= 0 ) { 
        //
        // set the status and severity to normal
        //
        pDD->setStat ( epicsAlarmNone );
        pDD->setSevr ( epicsSevNone );

        //
        // set the time stamp to the last time that
        // we added bytes to the in buf
        //
        aitTimeStamp gddts = this->lastRecvTS;
        pDD->setTimeStamp ( & gddts );

        //
        // call the server tool's virtual function
        //
        status = ( this->ctx.getChannel()->*pWriteMethod ) ( this->ctx, *pDD );
    }
    else {
        status = S_cas_noConvert;
    }

    gddStat = pDD->unreference ();
    assert ( ! gddStat );

    return status;
}

//
// casStrmClient::read()
//
caStatus casStrmClient::read ()
{
    const caHdrLargeArray * pHdr = this->ctx.getMsg();

    {
        gdd * pDD = 0;
        caStatus status = createDBRDD ( pHdr->m_dataType, pHdr->m_count,
                this->ctx.getChannel()->getMaxElem(), pDD );
        if ( status != S_cas_success ) {
            return status;
        }
        pValueRead.set ( pDD );
        pDD->unreference ();
    }

    //
    // clear the async IO flag
    //
    this->userStartedAsyncIO = false;

    {
        //
        // call the server tool's virtual function
        //
        caStatus servStat = this->ctx.getChannel()->
                        read ( this->ctx, *pValueRead );

        //
        // prevent problems when they initiate
        // async IO but dont return status
        // indicating so (and vise versa)
        //
        if ( this->userStartedAsyncIO ) {
            if ( servStat != S_casApp_asyncCompletion ) {
                errlogPrintf (
                    "Application returned %d from casChannel::read() - "
                    "expected S_casApp_asyncCompletion\n",
                    servStat );
                servStat = S_casApp_asyncCompletion;
            }
            pValueRead.set ( 0 );
        }
        else if ( servStat == S_casApp_asyncCompletion ) {
            servStat = S_cas_badParameter;
            errMessage ( servStat, 
                "- expected asynch IO creation from casChannel::read()");
        }
        else if ( servStat != S_casApp_success ) {
            pValueRead.set ( 0 );
            if ( servStat == S_casApp_postponeAsyncIO ) {
                casPVI & pvi ( this->ctx.getChannel()->getPVI() );
                if ( pvi.ioIsPending () ) {
                    pvi.addItemToIOBLockedList ( *this );
                }
                else {
                    // Its not ok to postpone IO when there isnt at 
                    // least one request pending. In that situation
                    // there is no event from the service telling us 
                    // when its ok to start issuing requests again!
                    // So in that situation we tell the client that
                    // the service refused the request, and this
                    // caused the request to fail.
                    this->issuePosponeWhenNonePendingWarning ( "read" );
                    servStat = S_cas_posponeWhenNonePending;
                }
            }
        }
        return servStat;
    }
}

//
// casStrmClient::userName() 
//
void casStrmClient::userName ( char * pBuf, unsigned bufSize ) const
{
    if ( bufSize ) {
        const char *pName = this->pUserName ? this->pUserName : "?";
        strncpy ( pBuf, pName, bufSize );
        pBuf [bufSize-1] = '\0';
    }
}

//
// caServerI::roomForNewChannel()
//
inline bool caServerI::roomForNewChannel() const
{
    return true;
}

//
//  casStrmClient::xSend()
//
outBufClient::flushCondition casStrmClient ::
    xSend ( char * pBufIn, bufSizeT nBytesToSend, bufSizeT & nBytesSent )
{
    return this->osdSend ( pBufIn, nBytesToSend, nBytesSent );
}

//
// casStrmClient::xRecv()
//
inBufClient::fillCondition casStrmClient::xRecv ( char * pBufIn, bufSizeT nBytesToRecv,
                                 inBufClient::fillParameter, bufSizeT & nActualBytes )
{
    inBufClient::fillCondition stat = 
        this->osdRecv ( pBufIn, nBytesToRecv, nActualBytes );
    //
    // this is used to set the time stamp for write GDD's
    //
    this->lastRecvTS = epicsTime::getCurrent ();
    return stat;
}

//
// casStrmClient::getDebugLevel()
//
unsigned casStrmClient::getDebugLevel () const
{
    return this->getCAS().getDebugLevel ();
}

//
// casStrmClient::casMonitorCallBack()
//
caStatus casStrmClient::casMonitorCallBack ( 
    epicsGuard < casClientMutex > & guard,
    casMonitor & mon, const gdd & value )
{
    return mon.response ( guard, *this, value );
}

//
//  casStrmClient::sendErr()
//
caStatus casStrmClient::sendErr ( epicsGuard <casClientMutex> &,
    const caHdrLargeArray * curp, ca_uint32_t cid, 
    const int reportedStatus, const char *pformat, ... )
{
    unsigned stringSize;
    char msgBuf[1024]; /* allocate plenty of space for the message string */
    if ( pformat ) {
        va_list args;
        va_start ( args, pformat );
        int status = vsprintf (msgBuf, pformat, args);
        if ( status < 0 ) {
            errPrintf (S_cas_internal, __FILE__, __LINE__,
                "bad sendErr(%s)", pformat);
            stringSize = 0u;
        }
        else {
            stringSize = 1u + (unsigned) status;
        }
        va_end ( args );
    }
    else {
        stringSize = 0u;
    }

    unsigned hdrSize = sizeof ( caHdr );
    if ( ( curp->m_postsize >= 0xffff || curp->m_count >= 0xffff ) && 
            CA_V49( this->minor_version_number ) ) {
        hdrSize += 2 * sizeof ( ca_uint32_t );
    }

    caHdr * pReqOut;
    caStatus status = this->out.copyInHeader ( CA_PROTO_ERROR, 
        hdrSize + stringSize, 0, 0, cid, reportedStatus,
        reinterpret_cast <void **> ( & pReqOut ) );
    if ( ! status ) {
        char * pMsgString;

        /*
         * copy back the request protocol
         * (in network byte order)
         */
        if ( ( curp->m_postsize >= 0xffff || curp->m_count >= 0xffff ) && 
                CA_V49( this->minor_version_number ) ) {
            ca_uint32_t *pLW = ( ca_uint32_t * ) ( pReqOut + 1 );
            pReqOut->m_cmmd = htons ( curp->m_cmmd );
            pReqOut->m_postsize = htons ( 0xffff );
            pReqOut->m_dataType = htons ( curp->m_dataType );
            pReqOut->m_count = htons ( 0u );
            pReqOut->m_cid = htonl ( curp->m_cid );
            pReqOut->m_available = htonl ( curp->m_available );
            pLW[0] = htonl ( curp->m_postsize );
            pLW[1] = htonl ( curp->m_count );
            pMsgString = ( char * ) ( pLW + 2 );
        }
        else {
            pReqOut->m_cmmd = htons (curp->m_cmmd);
            pReqOut->m_postsize = htons ( ( (ca_uint16_t) curp->m_postsize ) );
            pReqOut->m_dataType = htons (curp->m_dataType);
            pReqOut->m_count = htons ( ( (ca_uint16_t) curp->m_count ) );
            pReqOut->m_cid = htonl (curp->m_cid);
            pReqOut->m_available = htonl (curp->m_available);
            pMsgString = ( char * ) ( pReqOut + 1 );
         }

        /*
         * add their context string into the protocol
         */
        memcpy ( pMsgString, msgBuf, stringSize );

        this->out.commitMsg ();
    }

    return S_cas_success;
}

// send minor protocol revision to the client
void casStrmClient::sendVersion ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    caStatus status = this->out.copyInHeader ( CA_PROTO_VERSION, 0, 
        0, CA_MINOR_PROTOCOL_REVISION, 0, 0, 0 );
    if ( ! status ) {
        this->out.commitMsg ();
    }
}

bool casStrmClient::inBufFull () const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    return this->in.full ();
}

inBufClient::fillCondition casStrmClient::inBufFill ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    return this->in.fill ();
}

bufSizeT casStrmClient :: inBufBytesPending () const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    return this->in.bytesPresent ();
}

bufSizeT casStrmClient :: 
    outBufBytesPending () const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    return this->out.bytesPresent ();
}

outBufClient::flushCondition casStrmClient::flush ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    return this->out.flush ();
}

//
// casStrmClient::logBadIdWithFileAndLineno()
//
caStatus casStrmClient::logBadIdWithFileAndLineno ( 
    epicsGuard < casClientMutex > & guard,
    const caHdrLargeArray * mp, const void * dp, 
    const int cacStatus, const char * pFileName, 
    const unsigned lineno, const unsigned idIn )
{
    if ( pFileName ) {
        caServerI::dumpMsg ( this->pHostName, this->pUserName, mp, dp, 
            "bad resource id in \"%s\" at line %d\n",
            pFileName, lineno );
    }
    else {
        caServerI::dumpMsg ( this->pHostName, this->pUserName, mp, dp, 
            "bad resource id\n" );
    }

    int status = this->sendErr ( guard,
            mp, invalidResID, cacStatus, 
            "Bad Resource ID=%u detected at %s.%d",
            idIn, pFileName, lineno );

    return status;
}

/*
 * casStrmClient::sendErrWithEpicsStatus()
 *
 * same as sendErr() except that we convert epicsStatus
 * to a string and send that additional detail
 */
caStatus casStrmClient::sendErrWithEpicsStatus ( 
    epicsGuard < casClientMutex > & guard, const caHdrLargeArray * pMsg, 
    ca_uint32_t cid, caStatus epicsStatus, caStatus clientStatus )
{
    char buf[0x1ff];
    errSymLookup ( epicsStatus, buf, sizeof(buf) );
    return this->sendErr ( guard, pMsg, cid, clientStatus, buf );
}

