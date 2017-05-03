/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Jeffrey O. Hill <johill@lanl.gov>
 *
 *          Ralph Lange <Ralph.Lange@bessy.de>
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "errlog.h"
#include "freeList.h"
#include "osiPoolStatus.h"
#include "osiSock.h"

#include "caerr.h"
#include "net_convert.h"

#define epicsExportSharedSymbols
#include "asDbLib.h"
#include "callback.h"
#include "db_access.h"
#include "db_access_routines.h"
#include "dbChannel.h"
#include "dbCommon.h"
#include "dbEvent.h"
#include "db_field_log.h"
#include "dbNotify.h"
#include "rsrv.h"
#include "server.h"
#include "special.h"

#define RECORD_NAME(CHAN) (dbChannelRecord(CHAN)->name)

static EVENTFUNC read_reply;

#define logBadId(CLIENT, MP, PPL)\
logBadIdWithFileAndLineno(CLIENT, MP, PPL, __FILE__, __LINE__)

/*
 * for tracking db put notifies
 */
typedef struct rsrv_put_notify {
    ELLNODE         node;
    processNotify       dbPutNotify;
    caHdrLargeArray msg;
    /*
     * Include a union of all scalar types
     * including fixed length strings so
     * that in many cases we can avoid
     * allocating another buffer and only
     * use an rsrv_put_notify from its
     * free list.
     */
    union {
        dbr_string_t strval;
        dbr_short_t shrtval;
        dbr_short_t intval;
        dbr_float_t fltval;
        dbr_enum_t enmval;
        dbr_char_t charval;
        dbr_long_t longval;
        dbr_double_t doubleval;
    } dbrScalarValue;
    /* arguments for db_put_field */
    void *pbuffer;
    long nRequest;
    short dbrType;
    /* end arguments for db_put_field */
    void            * asWritePvt;
    unsigned        valueSize; /* size of block pointed to by pbuffer */
    char            busy; /* put notify in progress */
    char            onExtraLaborQueue;
} RSRVPUTNOTIFY;

/*
 * casCalloc()
 *
 * (dont drop below some max block threshold)
 */
static void *casCalloc(size_t count, size_t size)
{
    if ( UINT_MAX / size >= count ) {
        if (!osiSufficentSpaceInPool(size*count)) {
            return NULL;
        }
        return calloc(count, size);
    }
    else {
        return NULL;
    }
}

/*
 * MPTOPCIU()
 *
 * used to be a macro
 */
static struct channel_in_use *MPTOPCIU (const caHdrLargeArray *mp)
{
    struct channel_in_use   *pciu;
    const unsigned      id = mp->m_cid;

    LOCK_CLIENTQ;
    pciu = bucketLookupItemUnsignedId (pCaBucket, &id);
    UNLOCK_CLIENTQ;

    return pciu;
}

/*  vsend_err()
 *
 *  reflect error msg back to the client
 *
 *  send buffer lock must be on while in this routine
 *
 */
static void vsend_err(
const caHdrLargeArray   *curp,
int                     status,
struct client           *client,
const char              *pformat,
va_list                 args
)
{
    static const ca_uint32_t    maxDiagLen = 512;
    struct channel_in_use       *pciu;
    caHdr                       *pReqOut;
    char                        *pMsgString;
    ca_uint32_t                 size;
    ca_uint32_t                 cid;
    int                         localStatus;

    switch ( curp->m_cmmd ) {
    case CA_PROTO_EVENT_ADD:
    case CA_PROTO_EVENT_CANCEL:
    case CA_PROTO_READ:
    case CA_PROTO_READ_NOTIFY:
    case CA_PROTO_WRITE:
    case CA_PROTO_WRITE_NOTIFY:
        pciu = MPTOPCIU(curp);
        if(pciu){
            cid = pciu->cid;
        }
        else{
            cid = 0xffffffff;
        }
        break;

    case CA_PROTO_SEARCH:
        cid = curp->m_cid;
        break;

    case CA_PROTO_EVENTS_ON:
    case CA_PROTO_EVENTS_OFF:
    case CA_PROTO_READ_SYNC:
    case CA_PROTO_SNAPSHOT:
    default:
        cid = 0xffffffff;
        break;
    }

    /*
     * allocate plenty of space for a sprintf() buffer
     */
    localStatus = cas_copy_in_header ( client,
        CA_PROTO_ERROR, maxDiagLen, 0, 0, cid, status,
        ( void * ) &pReqOut );
    if ( localStatus != ECA_NORMAL ) {
        errlogPrintf ( "caserver: Unable to deliver err msg \"%s\" to client because \"%s\"\n",
            ca_message (status), ca_message (localStatus) );
        errlogVprintf ( pformat, args );
        return;
    }

    /*
     * copy back the request protocol
     * (in network byte order)
     */
    if ( ( curp->m_postsize >= 0xffff || curp->m_count >= 0xffff ) &&
            CA_V49( client->minor_version_number ) ) {
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
        size = sizeof ( caHdr ) + 2 * sizeof ( *pLW );
    }
    else {
        pReqOut->m_cmmd = htons (curp->m_cmmd);
        pReqOut->m_postsize = htons ( ( (ca_uint16_t) curp->m_postsize ) );
        pReqOut->m_dataType = htons (curp->m_dataType);
        pReqOut->m_count = htons ( ( (ca_uint16_t) curp->m_count ) );
        pReqOut->m_cid = htonl (curp->m_cid);
        pReqOut->m_available = htonl (curp->m_available);
        pMsgString = ( char * ) ( pReqOut + 1 );
        size = sizeof ( caHdr );
    }

    /*
     * add their context string into the protocol
     */
    localStatus = epicsVsnprintf ( pMsgString, maxDiagLen, pformat, args );
    if ( localStatus >= 1 ) {
        unsigned diagLen = ( unsigned ) localStatus;
        if ( diagLen < maxDiagLen ) {
            size += (ca_uint32_t) (diagLen + 1u);
        }
        else {
            errlogPrintf (
                "caserver: vsend_err: epicsVsnprintf detected "
                "error message truncation, pFormat = \"%s\"\n",
                pformat );
            size += maxDiagLen;
            pMsgString [ maxDiagLen - 1 ] = '\0';
        }
    }
    cas_commit_msg ( client, size );
}

/*  send_err()
 *
 *  reflect error msg back to the client
 *
 *  send buffer lock must be on while in this routine
 *
 */
static void send_err (
const caHdrLargeArray   *curp,
int                     status,
struct client           *client,
const char              *pformat,
                        ... )
{
    va_list args;
    va_start ( args, pformat );
    vsend_err ( curp, status, client, pformat, args );
    va_end ( args );
}

/*  log_header()
 *
 *  Debug aid - print the header part of a message.
 *
 */
static void log_header (
    const char              *pContext,
    struct client           *client,
    const caHdrLargeArray   *mp,
    const void              *pPayLoad,
    unsigned                mnum
)
{
    struct channel_in_use *pciu;
    char hostName[256];

    ipAddrToDottedIP (&client->addr, hostName, sizeof(hostName));

    pciu = MPTOPCIU(mp);

    if (pContext) {
        epicsPrintf ("CAS: request from %s => %s\n",
            hostName, pContext);
    }

    epicsPrintf ( "CAS: Request from %s => cmmd=%d cid=0x%x type=%d count=%d postsize=%u\n",
        hostName, mp->m_cmmd, mp->m_cid, mp->m_dataType, mp->m_count, mp->m_postsize);

    epicsPrintf ( "CAS: Request from %s =>   available=0x%x \tN=%u paddr=%p\n",
        hostName, mp->m_available, mnum, (pciu ? (void *)&pciu->dbch : NULL));

    if (mp->m_cmmd==CA_PROTO_WRITE && mp->m_dataType==DBF_STRING && pPayLoad ) {
        epicsPrintf ( "CAS: Request from %s =>   Wrote string \"%s\"\n",
        hostName, (char *)pPayLoad );
    }
}

/*
 * logBadIdWithFileAndLineno()
 */
static void logBadIdWithFileAndLineno(
struct client       *client,
caHdrLargeArray     *mp,
const void          *pPayload,
char                *pFileName,
unsigned            lineno
)
{
    log_header ( "bad resource ID", client, mp, pPayload, 0 );
    SEND_LOCK ( client );
    send_err ( mp, ECA_INTERNAL, client, "Bad Resource ID at %s.%d",
        pFileName, lineno );
    SEND_UNLOCK ( client );
}

/*
 * bad_udp_cmd_action()
 */
static int bad_udp_cmd_action ( caHdrLargeArray *mp,
                       void *pPayload, struct client *pClient )
{
    if (CASDEBUG > 0)
        log_header ("invalid (damaged?) request code from UDP",
                    pClient, mp, pPayload, 0);
    return RSRV_ERROR;
}

/*
 * bad_tcp_cmd_action()
 */
static int bad_tcp_cmd_action ( caHdrLargeArray *mp, void *pPayload,
                           struct client *client )
{
    const char *pCtx = "invalid (damaged?) request code from TCP";
    log_header ( pCtx, client, mp, pPayload, 0 );

    /*
     *  by default, clients dont recover
     *  from this
     */
    SEND_LOCK (client);
    send_err (mp, ECA_INTERNAL, client, pCtx);
    SEND_UNLOCK (client);

    return RSRV_ERROR;
}

/*
 * tcp_version_action()
 */
static int tcp_version_action ( caHdrLargeArray *mp, void *pPayload,
                           struct client *client )
{
    double tmp;
    unsigned epicsPriorityNew;
    unsigned epicsPrioritySelf;

    client->minor_version_number = mp->m_count;

    if (!CA_VSUPPORTED(mp->m_count)) {
        DLOG ( 2, ( "CAS: Ignore version from unsupported client %u\n", mp->m_count ) );
        return RSRV_ERROR;
    }

    if ( mp->m_dataType > CA_PROTO_PRIORITY_MAX ) {
        return RSRV_ERROR;
    }

    tmp = mp->m_dataType - CA_PROTO_PRIORITY_MIN;
    tmp *= epicsThreadPriorityCAServerHigh - epicsThreadPriorityCAServerLow;
    tmp /= CA_PROTO_PRIORITY_MAX - CA_PROTO_PRIORITY_MIN;
    tmp += epicsThreadPriorityCAServerLow;
    epicsPriorityNew = (unsigned) tmp;
    epicsPrioritySelf = epicsThreadGetPrioritySelf();
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
    return RSRV_OK;
}

/*
 * tcp_echo_action()
 */
static int tcp_echo_action ( caHdrLargeArray *mp,
                       void *pPayload, struct client *pClient )
{
    char *pPayloadOut;
    int status;
    SEND_LOCK ( pClient );
    status = cas_copy_in_header ( pClient, mp->m_cmmd, mp->m_postsize,
        mp->m_dataType, mp->m_count, mp->m_cid, mp->m_available,
        ( void * ) &pPayloadOut );
    if ( status == ECA_NORMAL ) {
        memcpy ( pPayloadOut, pPayload, mp->m_postsize );
        cas_commit_msg ( pClient, mp->m_postsize );
    }
    SEND_UNLOCK ( pClient );
    return RSRV_OK;
}

/*
 * events_on_action ()
 */
static int events_on_action ( caHdrLargeArray *mp,
                       void *pPayload, struct client *pClient )
{
    db_event_flow_ctrl_mode_off ( pClient->evuser );
    return RSRV_OK;
}

/*
 * events_off_action ()
 */
static int events_off_action ( caHdrLargeArray *mp,
                       void *pPayload, struct client *pClient )
{
    db_event_flow_ctrl_mode_on ( pClient->evuser );
    return RSRV_OK;
}

/*
 * no_read_access_event()
 *
 * !! LOCK needs to applied by caller !!
 *
 * substantial complication introduced here by the need for backwards
 * compatibility
 */
static void no_read_access_event ( struct client *pClient,
    struct event_ext *pevext )
{
    char *pPayloadOut;
    int status;

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
    status = cas_copy_in_header ( pClient, pevext->msg.m_cmmd, pevext->size,
        pevext->msg.m_dataType, pevext->msg.m_count, ECA_NORDACCESS,
        pevext->msg.m_available, ( void * ) &pPayloadOut );
    if ( status == ECA_NORMAL ) {
        memset ( pPayloadOut, 0, pevext->size );
        cas_commit_msg ( pClient, pevext->size );
    }
    else {
        send_err ( &pevext->msg, status, pClient,
            "server unable to load read access denied response into protocol buffer PV=\"%s max bytes=%u\"",
            RECORD_NAME ( pevext->pciu->dbch ), rsrvSizeofLargeBufTCP );
    }
}

/*
 *  read_reply()
 */
static void read_reply ( void *pArg, struct dbChannel *dbch,
                       int eventsRemaining, db_field_log *pfl )
{
    ca_uint32_t cid;
    void *pPayload;
    struct event_ext *pevext = pArg;
    struct client *pClient = pevext->pciu->client;
    struct channel_in_use *pciu = pevext->pciu;
    const int readAccess = asCheckGet ( pciu->asClientPVT );
    int status;
    int autosize;
    int local_fl = 0;
    long item_count;
    ca_uint32_t payload_size;
    dbAddr *paddr=&dbch->addr;

    SEND_LOCK ( pClient );

    cid = ECA_NORMAL;

    /* If the client has requested a zero element count we interpret this as a
     * request for all avaiable elements.  In this case we initialise the
     * header with the maximum element size specified by the database. */
    autosize = pevext->msg.m_count == 0;
    item_count =
        autosize ? paddr->no_elements : pevext->msg.m_count;
    payload_size = dbr_size_n(pevext->msg.m_dataType, item_count);
    status = cas_copy_in_header(
        pClient, pevext->msg.m_cmmd, payload_size,
        pevext->msg.m_dataType, item_count, cid, pevext->msg.m_available,
        &pPayload );
    if ( status != ECA_NORMAL ) {
        send_err ( &pevext->msg, status, pClient,
            "server unable to load read (or subscription update) response "
            "into protocol buffer PV=\"%s\" max bytes=%u",
            RECORD_NAME ( dbch ), rsrvSizeofLargeBufTCP );
        if ( ! eventsRemaining )
            cas_send_bs_msg ( pClient, FALSE );
        SEND_UNLOCK ( pClient );
        return;
    }

    /*
     * verify read access
     */
    if ( ! readAccess ) {
        no_read_access_event ( pClient, pevext );
        if ( ! eventsRemaining )
            cas_send_bs_msg ( pClient, FALSE );
        SEND_UNLOCK ( pClient );
        return;
    }

    /* If filters are involved in a read, create field log and run filters */
    if (!pfl && (ellCount(&dbch->pre_chain) || ellCount(&dbch->post_chain))) {
        pfl = db_create_read_log(dbch);
        if (pfl) {
            local_fl = 1;
            pfl = dbChannelRunPreChain(dbch, pfl);
            pfl = dbChannelRunPostChain(dbch, pfl);
        }
    }

    status = dbChannel_get_count ( dbch, pevext->msg.m_dataType,
                  pPayload, &item_count, pfl);

    if (local_fl) db_delete_field_log(pfl);

    if ( status < 0 ) {
        /* Clients recv the status of the operation directly to the
         * event/put/get callback.  (from CA_V41())
         *
         * Fetched value is set to zero in case they use it even when the
         * status indicates failure -- unless the client selected autosizing
         * data, in which case they'd better know what they're doing!
         *
         * The m_cid field in the protocol header is abused to carry the
         * status */
        if (autosize) {
            payload_size = dbr_size_n(pevext->msg.m_dataType, 0);
            cas_set_header_count(pClient, 0);
        }
        memset ( pPayload, 0, payload_size );
        cas_set_header_cid ( pClient, ECA_GETFAIL );
        cas_commit_msg ( pClient, payload_size );
    }
    else {
        int cacStatus = caNetConvert (
            pevext->msg.m_dataType, pPayload, pPayload,
            TRUE /* host -> net format */, item_count );
        if ( cacStatus == ECA_NORMAL ) {
            ca_uint32_t data_size =
                dbr_size_n(pevext->msg.m_dataType, item_count);
            if (autosize) {
                payload_size = data_size;
                cas_set_header_count(pClient, item_count);
            }
            else if (payload_size > data_size)
                memset(
                    (char *) pPayload + data_size, 0, payload_size - data_size);
        }
        else {
            if (autosize) {
                payload_size = dbr_size_n(pevext->msg.m_dataType, 0);
                cas_set_header_count(pClient, 0);
            }
            memset ( pPayload, 0, payload_size );
            cas_set_header_cid ( pClient, cacStatus );
        }
        cas_commit_msg ( pClient, payload_size );
    }

    /*
     * Ensures timely response for events, but does queue 
     * them up like db requests when the OPI does not keep up.
     */
    if ( ! eventsRemaining )
        cas_send_bs_msg ( pClient, FALSE );

    SEND_UNLOCK ( pClient );

    return;
}

/*
 * read_action ()
 */
static int read_action ( caHdrLargeArray *mp, void *pPayloadIn, struct client *pClient )
{
    struct channel_in_use *pciu = MPTOPCIU ( mp );
    int readAccess;
    ca_uint32_t payloadSize;
    void *pPayload;
    int status;
    int local_fl = 0;
    db_field_log *pfl = NULL;

    if ( ! pciu ) {
        logBadId ( pClient, mp, 0 );
        return RSRV_ERROR;
    }
    readAccess = asCheckGet ( pciu->asClientPVT );

    SEND_LOCK ( pClient );

    if ( INVALID_DB_REQ ( mp->m_dataType ) ) {
        send_err ( mp, ECA_BADTYPE, pClient, RECORD_NAME ( pciu->dbch ) );
        SEND_UNLOCK ( pClient );
        return RSRV_ERROR;
    }

    payloadSize = dbr_size_n ( mp->m_dataType, mp->m_count );
    status = cas_copy_in_header ( pClient, mp->m_cmmd, payloadSize,
        mp->m_dataType, mp->m_count, pciu->cid, mp->m_available, &pPayload );
    if ( status != ECA_NORMAL ) {
        send_err ( mp, status, pClient,
            "server unable to load read response into protocol buffer PV=\"%s\" max bytes=%u",
            RECORD_NAME ( pciu->dbch ), rsrvSizeofLargeBufTCP );
        SEND_UNLOCK ( pClient );
        return RSRV_OK;
    }

    /*
     * verify read access
     */
    if ( ! readAccess ) {
        status = ECA_NORDACCESS;
        send_err ( mp, status,
            pClient, RECORD_NAME ( pciu->dbch ) );
        SEND_UNLOCK ( pClient );
        return RSRV_OK;
    }

    /* If filters are involved in a read, create field log and run filters */
    if (ellCount(&pciu->dbch->pre_chain) || ellCount(&pciu->dbch->post_chain)) {
        pfl = db_create_read_log(pciu->dbch);
        if (pfl) {
            local_fl = 1;
            pfl = dbChannelRunPreChain(pciu->dbch, pfl);
            pfl = dbChannelRunPostChain(pciu->dbch, pfl);
        }
    }

    status = dbChannel_get ( pciu->dbch, mp->m_dataType,
                  pPayload, mp->m_count, pfl );

    if (local_fl) db_delete_field_log(pfl);

    if ( status < 0 ) {
        send_err ( mp, ECA_GETFAIL, pClient, RECORD_NAME ( pciu->dbch ) );
        SEND_UNLOCK ( pClient );
        return RSRV_OK;
    }

    status = caNetConvert (
        mp->m_dataType, pPayload, pPayload,
        TRUE /* host -> net format */, mp->m_count );
	if ( status != ECA_NORMAL ) {
        send_err ( mp, status, pClient, RECORD_NAME ( pciu->dbch ) );
        SEND_UNLOCK ( pClient );
        return RSRV_OK;
    }

    /*
     * force string message size to be the true size rounded to even
     * boundary
     */
    if ( mp->m_dataType == DBR_STRING && mp->m_count == 1 ) {
        char * pStr = (char *) pPayload;
        size_t strcnt = epicsStrnLen( pStr, payloadSize );
        if ( strcnt < payloadSize ) {
            payloadSize = ( ca_uint32_t ) ( strcnt + 1u );
        }
        else {
            pStr[payloadSize-1] = '\0';
            errlogPrintf (
                "caserver: read_action: detected DBR_STRING w/o nill termination "
                "in response from db_get_field, pPayload = \"%s\"\n",
                pStr );
        }
    }
    cas_commit_msg ( pClient, payloadSize );

    SEND_UNLOCK ( pClient );

    return RSRV_OK;
}

/*
 * read_notify_action()
 */
static int read_notify_action ( caHdrLargeArray *mp, void *pPayload, struct client *client )
{
    struct channel_in_use *pciu;
    struct event_ext evext;

    if ( INVALID_DB_REQ(mp->m_dataType) ) {
        return RSRV_ERROR;
    }

    pciu = MPTOPCIU ( mp );
    if ( !pciu ) {
        logBadId ( client, mp, pPayload );
        return RSRV_ERROR;
    }

    evext.msg = *mp;
    evext.pciu = pciu;
    evext.pdbev = NULL;
    evext.size = dbr_size_n ( mp->m_dataType, mp->m_count );

    /*
     * Arguments to this routine organized in
     * favor of the standard db event calling
     * mechanism-  routine(userarg, paddr). See
     * events added above.
     *
     * Hold argument set true so the send message
     * buffer is not flushed once each call.
     */
    read_reply ( &evext, pciu->dbch, TRUE, NULL );

    return RSRV_OK;
}

/*
 * write_action()
 */
static int write_action ( caHdrLargeArray *mp,
                        void *pPayload, struct client *client )
{
    struct channel_in_use   *pciu;
    int                     status;
    long                    dbStatus;
    void                    *asWritePvt;

    pciu = MPTOPCIU(mp);
    if(!pciu){
        logBadId(client, mp, pPayload);
        return RSRV_ERROR;
    }

    if(!rsrvCheckPut(pciu)){
        status = ECA_NOWTACCESS;
        SEND_LOCK(client);
        send_err(
            mp,
            status,
            client,
            RECORD_NAME ( pciu->dbch ));
        SEND_UNLOCK(client);
        return RSRV_OK;
    }

    status = caNetConvert (
        mp->m_dataType, pPayload, pPayload,
        FALSE /* net -> host format */, mp->m_count );
	if ( status != ECA_NORMAL ) {
        log_header ("invalid data type", client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(
            mp,
            status,
            client,
            RECORD_NAME ( pciu->dbch ));
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    asWritePvt = asTrapWriteWithData ( pciu->asClientPVT,
        pciu->client->pUserName ? pciu->client->pUserName : "",
        pciu->client->pHostName ? pciu->client->pHostName : "",
        pciu->dbch, mp->m_dataType, mp->m_count, pPayload );

    dbStatus = dbChannel_put(
                  pciu->dbch,
                  mp->m_dataType,
                  pPayload,
                  mp->m_count);

    asTrapWriteAfter(asWritePvt);

    if (dbStatus < 0) {
        SEND_LOCK(client);
        send_err(
            mp,
            ECA_PUTFAIL,
            client,
            RECORD_NAME ( pciu->dbch ));
        SEND_UNLOCK(client);
    }

    return RSRV_OK;
}

/*
 * host_name_action()
 */
static int host_name_action ( caHdrLargeArray *mp, void *pPayload,
    struct client *client )
{
    ca_uint32_t             size;
    char                    *pName;
    char                    *pMalloc;
    int                     chanCount;

    epicsMutexMustLock ( client->chanListLock );
    chanCount =
        ellCount ( &client->chanList ) +
        ellCount ( &client->chanPendingUpdateARList );
    epicsMutexUnlock( client->chanListLock );

    if ( chanCount != 0 ) {
        SEND_LOCK ( client );
        send_err(
            mp,
            ECA_INTERNAL,
            client,
            "attempts to use protocol to set host name "
            "after creating first channel ignored by server" );
        SEND_UNLOCK ( client );
        return RSRV_OK;
    }

    pName = (char *) pPayload;
    size = epicsStrnLen(pName, mp->m_postsize)+1;
    if (size > 512 || size > mp->m_postsize) {
        log_header ( "bad (very long) host name", 
            client, mp, pPayload, 0 );
        SEND_LOCK(client);
        send_err(
            mp,
            ECA_INTERNAL,
            client,
            "bad (very long) host name");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    /*
     * user name will not change if there isnt enough memory
     */
    pMalloc = malloc(size);
    if(!pMalloc){
        log_header ( "no space in pool for new host name",
            client, mp, pPayload, 0 );
        SEND_LOCK(client);
        send_err(
            mp,
            ECA_ALLOCMEM,
            client,
            "no space in pool for new host name");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }
    strncpy(
        pMalloc,
        pName,
        size-1);
    pMalloc[size-1]='\0';

    pName = client->pHostName;
    client->pHostName = pMalloc;
    if ( pName ) {
        free ( pName );
    }

    DLOG (2, ( "CAS: host_name_action for \"%s\"\n",
        client->pHostName ? client->pHostName : "" ) );

    return RSRV_OK;
}


/*
 * client_name_action()
 */
static int client_name_action ( caHdrLargeArray *mp, void *pPayload,
    struct client *client )
{
    ca_uint32_t             size;
    char                    *pName;
    char                    *pMalloc;
    int                     chanCount;

    epicsMutexMustLock ( client->chanListLock );
    chanCount =
        ellCount ( &client->chanList ) +
        ellCount ( &client->chanPendingUpdateARList );
    epicsMutexUnlock( client->chanListLock );

    if ( chanCount != 0 ) {
        SEND_LOCK ( client );
        send_err(
            mp,
            ECA_INTERNAL,
            client,
            "attempts to use protocol to set user name "
            "after creating first channel ignored by server" );
        SEND_UNLOCK ( client );
        return RSRV_OK;
    }

    pName = (char *) pPayload;
    size = epicsStrnLen(pName, mp->m_postsize)+1;
    if (size > 512 || size > mp->m_postsize) {
        log_header ("a very long user name was specified", 
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(
            mp,
            ECA_INTERNAL,
            client,
            "a very long user name was specified");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    /*
     * user name will not change if there isnt enough memory
     */
    pMalloc = malloc(size);
    if(!pMalloc){
        log_header ("no memory for new user name",
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(
            mp,
            ECA_ALLOCMEM,
            client,
            "no memory for new user name");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }
    strncpy(
        pMalloc,
        pName,
        size-1);
    pMalloc[size-1]='\0';

    pName = client->pUserName;
    client->pUserName = pMalloc;
    if ( pName ) {
        free ( pName );
    }

    return RSRV_OK;
}

/*
 * casCreateChannel ()
 */
static struct channel_in_use *casCreateChannel (
struct client   *client,
struct dbChannel *dbch,
unsigned    cid
)
{
    static unsigned     bucketID;
    unsigned        *pCID;
    struct channel_in_use   *pchannel;
    int         status;

    /* get block off free list if possible */
    pchannel = (struct channel_in_use *)
        freeListCalloc(rsrvChanFreeList);
    if (!pchannel) {
        return NULL;
    }
    ellInit(&pchannel->eventq);
    epicsTimeGetCurrent(&pchannel->time_at_creation);
    pchannel->dbch = dbch;
    pchannel->client = client;
    /*
     * bypass read only warning
     */
    pCID = (unsigned *) &pchannel->cid;
    *pCID = cid;

    /*
     * allocate a server id and enter the channel pointer
     * in the table
     *
     * NOTE: This detects the case where the PV id wraps
     * around and we attempt to have two resources on the same id.
     * The lock is applied here because on some architectures the
     * ++ operator isnt atomic.
     */
    LOCK_CLIENTQ;

    do {
        /*
         * bypass read only warning
         */
        pCID = (unsigned *) &pchannel->sid;
        *pCID = bucketID++;

        /*
         * Verify that this id is not in use
         */
        status = bucketAddItemUnsignedId (
                pCaBucket,
                &pchannel->sid,
                pchannel);
    } while (status == S_bucket_idInUse);

    if ( status == S_bucket_success ) {
        rsrvChannelCount++;
    }

    UNLOCK_CLIENTQ;

    if(status!=S_bucket_success){
        freeListFree(rsrvChanFreeList, pchannel);
        errMessage (status, "Unable to allocate server id");
        return NULL;
    }

    epicsMutexMustLock( client->chanListLock );
    pchannel->state = rsrvCS_pendConnectResp;
    ellAdd ( &client->chanList, &pchannel->node );
    epicsMutexUnlock ( client->chanListLock );

    return pchannel;
}

/*
 * casAccessRightsCB()
 *
 * If access right state changes then inform the client.
 * asLock is held
 */
static void casAccessRightsCB(ASCLIENTPVT ascpvt, asClientStatus type)
{
    struct client * pclient;
    struct channel_in_use * pciu;
    struct event_ext * pevext;

    pciu = asGetClientPvt(ascpvt);
    assert(pciu);

    pclient = pciu->client;
    assert(pclient);

    if(pclient->proto==IPPROTO_UDP){
        return;
    }

    switch(type)
    {
    case asClientCOAR:
        {
            const int readAccess = asCheckGet ( pciu->asClientPVT );
            unsigned sigReq = 0;

            epicsMutexMustLock ( pclient->chanListLock );
            if ( pciu->state == rsrvCS_pendConnectResp ) {
                ellDelete ( &pclient->chanList, &pciu->node );
                pciu->state = rsrvCS_pendConnectRespUpdatePendAR;
                ellAdd ( &pclient->chanPendingUpdateARList, &pciu->node );
                sigReq = 1;
            }
            else if ( pciu->state == rsrvCS_inService ) {
                ellDelete ( &pclient->chanList, &pciu->node );
                pciu->state = rsrvCS_inServiceUpdatePendAR;
                ellAdd ( &pclient->chanPendingUpdateARList, &pciu->node );
                sigReq = 1;
            }
            epicsMutexUnlock ( pclient->chanListLock );

            /*
             * Update all event call backs
             */
            epicsMutexMustLock(pclient->eventqLock);
            for (pevext = (struct event_ext *) ellFirst(&pciu->eventq);
                pevext;
                pevext = (struct event_ext *) ellNext(&pevext->node)){

                if ( pevext->pdbev ) {
                    if ( readAccess ){
                        db_event_enable ( pevext->pdbev );
                        db_post_single_event ( pevext->pdbev );
                    }
                    else {
                        db_post_single_event ( pevext->pdbev );
                        db_event_disable ( pevext->pdbev );
                    }
                }
            }
            epicsMutexUnlock(pclient->eventqLock);

            if ( sigReq ) {
                db_post_extra_labor( pclient->evuser );
            }

            break;
        }
    default:
        break;
    }
}

/*
 * access_rights_reply()
 */
static void access_rights_reply ( struct channel_in_use * pciu )
{
    unsigned        ar;
    int             status;

    assert ( pciu->client->proto!=IPPROTO_UDP );

    ar = 0; /* none */
    if ( asCheckGet ( pciu->asClientPVT ) ) {
        ar |= CA_PROTO_ACCESS_RIGHT_READ;
    }
    if ( rsrvCheckPut ( pciu ) ) {
        ar |= CA_PROTO_ACCESS_RIGHT_WRITE;
    }

    SEND_LOCK ( pciu->client );
    status = cas_copy_in_header (
        pciu->client, CA_PROTO_ACCESS_RIGHTS, 0,
        0, 0, pciu->cid, ar, 0 );
    /*
     * OK to just ignore the request if the connection drops
     */
    if ( status == ECA_NORMAL ) {
        cas_commit_msg ( pciu->client, 0u );
    }
    SEND_UNLOCK ( pciu->client );
}

/*
 * claim_ciu_reply()
 */
static void claim_ciu_reply ( struct channel_in_use * pciu )
{
    int status;
    ca_uint32_t nElem;
    long dbElem;

    access_rights_reply ( pciu );

    SEND_LOCK ( pciu->client );
    dbElem = dbChannelFinalElements(pciu->dbch);
    if ( dbElem < 0 ) {
        nElem = 0;
    }
    else {
        if ( ! CA_V49 ( pciu->client->minor_version_number ) ) {
            if ( dbElem >= 0xffff ) {
                nElem = 0xfffe;
            }
            else {
                nElem = (ca_uint32_t) dbElem;
            }
        }
        else {
            nElem = (ca_uint32_t) dbElem;
        }
    }
    status = cas_copy_in_header (
        pciu->client, CA_PROTO_CREATE_CHAN, 0u,
        dbChannelFinalCAType(pciu->dbch), nElem, pciu->cid,
        pciu->sid, NULL );
    if ( status == ECA_NORMAL ) {
        cas_commit_msg ( pciu->client, 0u );
    }
    SEND_UNLOCK(pciu->client);
}

/*
 * claim_ciu_action()
 */
static int claim_ciu_action ( caHdrLargeArray *mp,
                            void *pPayload, client *client )
{
    int status;
    struct channel_in_use *pciu;
    struct dbChannel *dbch;
    char *pName = (char *) pPayload;

    /*
     * The available field is used (abused)
     * here to communicate the miner version number
     * starting with CA 4.1. The field was set to zero
     * prior to 4.1
     */
    client->minor_version_number = mp->m_available;

    if (!CA_V44(client->minor_version_number))
        return RSRV_ERROR; /* shouldn't actually get here due to VSUPPORTED test in camessage() */

    /*
     * check the sanity of the message
     */
    if (mp->m_postsize<=1) {
        log_header ( "empty PV name in UDP search request?",
            client, mp, pPayload, 0 );
        return RSRV_OK;
    }
    pName[mp->m_postsize-1] = '\0';

    dbch = dbChannel_create (pName);
    if (!dbch) {
        SEND_LOCK(client);
        status = cas_copy_in_header ( client,
                                      CA_PROTO_CREATE_CH_FAIL, 0, 0, 0, mp->m_cid, 0, NULL );
        if (status == ECA_NORMAL)
            cas_commit_msg ( client, 0u );
        SEND_UNLOCK(client);
        return RSRV_OK;
    }

    DLOG ( 2, ("CAS: claim_ciu_action found '%s', type %d, count %d\n",
        pName, dbChannelCAType(dbch), dbChannelElements(dbch)) );

    pciu = casCreateChannel (
            client,
            dbch,
            mp->m_cid);
    if (!pciu) {
        log_header ("no memory to create new channel",
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(mp,
            ECA_ALLOCMEM,
            client,
            RECORD_NAME(dbch));
        SEND_UNLOCK(client);
        dbChannelDelete(dbch);
        return RSRV_ERROR;
    }

    /*
     * set up access security for this channel
     */
    status = asAddClient(
            &pciu->asClientPVT,
            asDbGetMemberPvt(pciu->dbch),
            asDbGetAsl(pciu->dbch),
            client->pUserName ? client->pUserName : "",
            client->pHostName ? client->pHostName : "");
    if(status != 0 && status != S_asLib_asNotActive){
        log_header ("No room for security table",
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(mp, ECA_ALLOCMEM, client, "No room for security table");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    /*
     * store ptr to channel in use block
     * in access security private
     */
    asPutClientPvt(pciu->asClientPVT, pciu);

    /*
     * register for asynch updates of access rights changes
     */
    status = asRegisterClientCallback(
            pciu->asClientPVT,
            casAccessRightsCB);
    if ( status == S_asLib_asNotActive ) {
        epicsMutexMustLock ( client->chanListLock );
        pciu->state = rsrvCS_inService;
        epicsMutexUnlock ( client->chanListLock );
        /*
         * force the initial AR update followed by claim response
         */
        claim_ciu_reply ( pciu );
    }
    else if (status!=0) {
        log_header ("No room for access security state change subscription",
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(mp, ECA_ALLOCMEM, client,
            "No room for access security state change subscription");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }
    return RSRV_OK;
}

/*
  * write_notify_put_callback()
  *
  * (called by the db call back thread)
  */
 LOCAL int write_notify_put_callback(processNotify *ppn,notifyPutType type)
 {
     struct channel_in_use * pciu = (struct channel_in_use *) ppn->usrPvt;
     struct rsrv_put_notify *pNotify;
 
     if(ppn->status==notifyCanceled) return 0;
 /*
  * No locking in this method because only a dbNotifyCancel could interrupt
  * and it does not return until cancel is done.
  */
     assert(pciu);
     assert(pciu->pPutNotify);
     pNotify = pciu->pPutNotify;
     return db_put_process(ppn,type,
         pNotify->dbrType,pNotify->pbuffer,pNotify->nRequest);
 }
 
 /*
  * write_notify_done_callback()
  *
  * (called by the db call back thread)
  */
 LOCAL void write_notify_done_callback(processNotify *ppn)
{
    struct channel_in_use * pciu = (struct channel_in_use *) ppn->usrPvt;
    struct client * pClient;

    /*
     * independent lock used here in order to
     * avoid any possibility of blocking
     * the database (or indirectly blocking
     * one client on another client).
     */
    assert(pciu);
    assert(pciu->pPutNotify);
    pClient = pciu->client;
    epicsMutexMustLock(pClient->putNotifyLock);

    if ( ! pciu->pPutNotify->busy || pciu->pPutNotify->onExtraLaborQueue ) {
        epicsMutexUnlock(pClient->putNotifyLock);
        errlogPrintf("Double DB put notify call back!!\n");
        return;
    }

    ellAdd(&pClient->putNotifyQue, &pciu->pPutNotify->node);
    pciu->pPutNotify->onExtraLaborQueue = TRUE;

    epicsMutexUnlock(pClient->putNotifyLock);

    /*
     * offload the labor for this to the
     * event task so that we never block
     * the db or another client.
     */
    db_post_extra_labor(pClient->evuser);
}

/*
 * write_notify_reply()
 * (called by the CA server event task via the extra labor interface)
 */
static void write_notify_reply ( struct client * pClient )
{
    while(TRUE){
        caHdrLargeArray msgtmp;
        void * asWritePvtTmp;
        ca_uint32_t status;
        int localStatus;

        /*
         * independent lock used here in order to
         * avoid any possibility of blocking
         * the database (or indirectly blocking
         * one client on another client).
         */
        epicsMutexMustLock(pClient->putNotifyLock);
        {
            RSRVPUTNOTIFY * ppnb = (RSRVPUTNOTIFY *)
                ellGet ( &pClient->putNotifyQue );
            if ( ! ppnb ) {
                epicsMutexUnlock(pClient->putNotifyLock);
                break;
            }
            /*
             *
             * Map from DB status to CA status
             *
             */
            if ( ppnb->dbPutNotify.status != notifyOK ) {
                status = ECA_PUTFAIL;
            }
            else{
                status = ECA_NORMAL;
            }
            msgtmp = ppnb->msg;
            asWritePvtTmp = ppnb->asWritePvt;
            ppnb->asWritePvt = 0;
            ppnb->onExtraLaborQueue = FALSE;
            ppnb->busy = FALSE;
        }
        epicsMutexUnlock(pClient->putNotifyLock);

        asTrapWriteAfter ( asWritePvtTmp );

        /*
         * the channel id field is being abused to carry
         * status here
         */
        SEND_LOCK(pClient);
        localStatus = cas_copy_in_header (
            pClient, CA_PROTO_WRITE_NOTIFY,
            0u, msgtmp.m_dataType, msgtmp.m_count, status,
            msgtmp.m_available, 0 );
        if ( localStatus != ECA_NORMAL ) {
            /*
             * inability to aquire buffer space
             * Indicates corruption
             */
            errlogPrintf("CA server corrupted - put call back(s) discarded\n");
            SEND_UNLOCK ( pClient );
            break;
        }

        /* commit the message */
        cas_commit_msg ( pClient, 0u );
        SEND_UNLOCK ( pClient );
    }

    /*
     * wakeup the TCP thread if it is waiting for a cb to complete
     */
    epicsEventSignal ( pClient->blockSem );
}

/*
 * sendAllUpdateAS()
 */
static void sendAllUpdateAS ( struct client *client )
{
    struct channel_in_use *pciu;

    epicsMutexMustLock ( client->chanListLock );

    pciu = ( struct channel_in_use * )
        ellGet ( & client->chanPendingUpdateARList );
    while ( pciu ) {
        if ( pciu->state == rsrvCS_pendConnectRespUpdatePendAR ) {
            claim_ciu_reply ( pciu );
        }
        else if ( pciu->state == rsrvCS_inServiceUpdatePendAR ) {
             access_rights_reply ( pciu );
        }
        else if ( pciu->state == rsrvCS_shutdown ) {
            /* no-op */
        }
        else {
            errlogPrintf (
            "%s at %d: corrupt channel state detected durring AR update\n",
                __FILE__, __LINE__);
        }
        pciu->state = rsrvCS_inService;
        ellAdd ( & client->chanList, & pciu->node );
        pciu = ( struct channel_in_use * )
            ellGet ( & client->chanPendingUpdateARList );
    }

    epicsMutexUnlock( client->chanListLock );
}

/*
 * rsrv_extra_labor()
 * (called by the CA server event task via the extra labor interface)
 */
void rsrv_extra_labor ( void * pArg )
{
    struct client * pClient = pArg;
    write_notify_reply ( pClient );
    sendAllUpdateAS ( pClient );
    cas_send_bs_msg ( pClient, TRUE );
}

/*
 * putNotifyErrorReply
 */
static void putNotifyErrorReply ( struct client *client, caHdrLargeArray *mp, int statusCA )
{
    int status;

    SEND_LOCK ( client );
    /*
     * the cid field abused to contain status
     * during put cb replies
     */
    status = cas_copy_in_header ( client, CA_PROTO_WRITE_NOTIFY,
        0u, mp->m_dataType, mp->m_count, statusCA,
        mp->m_available, 0 );
    if ( status != ECA_NORMAL ) {
        SEND_UNLOCK ( client );
        errlogPrintf ("%s at %d: should always get sufficent space for put notify error reply\n",
            __FILE__, __LINE__);
        return;
    }
    cas_commit_msg ( client, 0u );
    SEND_UNLOCK ( client );
}

void initializePutNotifyFreeList (void)
{
    if ( ! rsrvPutNotifyFreeList ) {
        freeListInitPvt ( &rsrvPutNotifyFreeList,
            sizeof(struct rsrv_put_notify), 512 );
        assert ( rsrvPutNotifyFreeList );
    }
}

static struct rsrv_put_notify *
    rsrvAllocPutNotify ( struct channel_in_use * pciu )
{
    struct rsrv_put_notify *pNotify;

    if ( rsrvPutNotifyFreeList ) {
        pNotify = (RSRVPUTNOTIFY *)
            freeListCalloc ( rsrvPutNotifyFreeList );
        if ( pNotify ) {
            pNotify->pbuffer = &pNotify->dbrScalarValue;
            pNotify->valueSize =
                    sizeof (pNotify->dbrScalarValue);
            pNotify->dbPutNotify.usrPvt = pciu;
            pNotify->dbPutNotify.chan = pciu->dbch;
            pNotify->dbPutNotify.putCallback =
                    write_notify_put_callback;
            pNotify->dbPutNotify.doneCallback =
                    write_notify_done_callback;
            pNotify->dbPutNotify.requestType = putProcessRequest;
        }
    }
    else {
        pNotify = NULL;
    }
    return pNotify;
}

static int rsrvExpandPutNotify (
    struct rsrv_put_notify * pNotify, unsigned sizeNeeded )
{
    int booleanStatus;

    if ( sizeNeeded > pNotify->valueSize ) {
        /*
         * try to use the union embeded in the free list
         * item, but allocate a random sized block if they
         * writing a vector.
         */
        if ( pNotify->valueSize >
            sizeof (pNotify->dbrScalarValue) ) {
            free ( pNotify->pbuffer );
        }
        pNotify->pbuffer = casCalloc(1,sizeNeeded);
        if ( pNotify->pbuffer ) {
            pNotify->valueSize = sizeNeeded;
            booleanStatus = TRUE;
        }
        else {
            /*
             * revert back to the embedded union
             */
            pNotify->pbuffer =
                &pNotify->dbrScalarValue;
            pNotify->valueSize =
                    sizeof (pNotify->dbrScalarValue);
            booleanStatus = FALSE;
        }
    }
    else {
        booleanStatus = TRUE;
    }

    return booleanStatus;
}

unsigned rsrvSizeOfPutNotify ( struct rsrv_put_notify *pNotify )
{
    unsigned size = sizeof ( *pNotify );
    if ( pNotify ) {
        if ( pNotify->valueSize >
                sizeof ( pNotify->dbrScalarValue ) ) {
            size += pNotify->valueSize;
        }
    }
    return size;
}

void rsrvFreePutNotify ( client *pClient,
        struct rsrv_put_notify *pNotify )
{
     if ( pNotify ) {
        char busyTmp;
        void * asWritePvtTmp = 0;

        epicsMutexMustLock ( pClient->putNotifyLock );
        busyTmp = pNotify->busy;
        epicsMutexUnlock ( pClient->putNotifyLock );

        /*
         * if any possiblity that the put notify is
         * outstanding then cancel it
         */
        if ( busyTmp ) {
            dbNotifyCancel ( &pNotify->dbPutNotify );
        }

        epicsMutexMustLock ( pClient->putNotifyLock );
        if ( pNotify->onExtraLaborQueue ) {
            ellDelete ( &pClient->putNotifyQue,
                        &pNotify->node );
        }
        busyTmp = pNotify->busy;
        asWritePvtTmp = pNotify->asWritePvt;
        pNotify->asWritePvt = 0;
        epicsMutexUnlock ( pClient->putNotifyLock );

        if ( busyTmp ) {
            asTrapWriteAfter ( asWritePvtTmp );
        }

        if ( pNotify->valueSize >
                sizeof(pNotify->dbrScalarValue) ) {
            free ( pNotify->pbuffer );
        }
        freeListFree ( rsrvPutNotifyFreeList, pNotify );
     }
}

/*
 * write_notify_action()
 */
static int write_notify_action ( caHdrLargeArray *mp, void *pPayload,
                               struct client  *client )
{
    unsigned size;
    int status;
    struct channel_in_use *pciu;

    pciu = MPTOPCIU(mp);
    if(!pciu){
        logBadId ( client, mp, pPayload );
        return RSRV_ERROR;
    }

    if (mp->m_dataType > LAST_BUFFER_TYPE) {
        log_header ("bad put notify data type", client, mp, pPayload, 0);
        putNotifyErrorReply (client, mp, ECA_BADTYPE);
        return RSRV_ERROR;
    }

    if(!rsrvCheckPut(pciu)){
        putNotifyErrorReply (client, mp, ECA_NOWTACCESS);
        return RSRV_OK;
    }

    size = dbr_size_n (mp->m_dataType, mp->m_count);

    if ( pciu->pPutNotify ) {

        /*
         * serialize concurrent put notifies
         */
        epicsMutexMustLock(client->putNotifyLock);
        while(pciu->pPutNotify->busy){
            epicsMutexUnlock(client->putNotifyLock);
            status = epicsEventWaitWithTimeout(client->blockSem,60.0);
            if ( status != epicsEventWaitOK ) {
                char busyTmp;
                void * asWritePvtTmp = 0;

                epicsMutexMustLock(client->putNotifyLock);
                busyTmp = pciu->pPutNotify->busy;
                epicsMutexUnlock(client->putNotifyLock);

                /*
                 * if any possibility of put notify still running
                 * then cancel it
                 */
                if ( busyTmp ) {
                    dbNotifyCancel(&pciu->pPutNotify->dbPutNotify);
                }
                epicsMutexMustLock(client->putNotifyLock);
                busyTmp = pciu->pPutNotify->busy;
                if ( busyTmp ) {
                    if ( pciu->pPutNotify->onExtraLaborQueue ) {
                        ellDelete ( &client->putNotifyQue,
                                    &pciu->pPutNotify->node );
                    }
                    pciu->pPutNotify->busy = FALSE;
                    asWritePvtTmp = pciu->pPutNotify->asWritePvt;
                    pciu->pPutNotify->asWritePvt = 0;
                }
                epicsMutexUnlock(client->putNotifyLock);

                if ( busyTmp ) {
                    log_header("put call back time out", client,
                        &pciu->pPutNotify->msg, pciu->pPutNotify->pbuffer, 0);
                    asTrapWriteAfter ( asWritePvtTmp );
                    putNotifyErrorReply (client, &pciu->pPutNotify->msg, ECA_PUTCBINPROG);
                }
            }
            epicsMutexMustLock(client->putNotifyLock);
        }
        epicsMutexUnlock(client->putNotifyLock);
    }
    else {
        pciu->pPutNotify = rsrvAllocPutNotify ( pciu );
        if ( ! pciu->pPutNotify ) {
            /*
             * send error and go to next request
             * if there isnt enough memory left
             */
            log_header ( "no memory to initiate put notify",
                client, mp, pPayload, 0 );
            putNotifyErrorReply (client, mp, ECA_ALLOCMEM);
            return RSRV_ERROR;
        }
    }

    if ( ! rsrvExpandPutNotify ( pciu->pPutNotify, size ) ) {
        log_header ( "no memory to initiate vector put notify",
            client, mp, pPayload, 0 );
        putNotifyErrorReply ( client, mp, ECA_ALLOCMEM );
        return RSRV_ERROR;
    }

    pciu->pPutNotify->busy = TRUE;
    pciu->pPutNotify->onExtraLaborQueue = FALSE;
    pciu->pPutNotify->msg = *mp;
    pciu->pPutNotify->nRequest = mp->m_count;

    status = caNetConvert (
        mp->m_dataType, pPayload, pciu->pPutNotify->pbuffer,
        FALSE /* net -> host format */, mp->m_count );
	if ( status != ECA_NORMAL ) {
        log_header ("invalid data type", client, mp, pPayload, 0);
        putNotifyErrorReply ( client, mp, status );
        return RSRV_ERROR;
    }

    pciu->pPutNotify->dbrType = mp->m_dataType;

    pciu->pPutNotify->asWritePvt = asTrapWriteWithData (
        pciu->asClientPVT,
        pciu->client->pUserName ? pciu->client->pUserName : "",
        pciu->client->pHostName ? pciu->client->pHostName : "",
        pciu->dbch, mp->m_dataType, mp->m_count,
        pciu->pPutNotify->pbuffer );

    dbProcessNotify(&pciu->pPutNotify->dbPutNotify);

    return RSRV_OK;
}

/*
 *
 * event_add_action()
 *
 */
static int event_add_action (caHdrLargeArray *mp, void *pPayload, struct client *client)
{
    struct mon_info *pmi = (struct mon_info *) pPayload;
    int spaceAvailOnFreeList;
    struct channel_in_use *pciu;
    struct event_ext *pevext;

    if ( INVALID_DB_REQ(mp->m_dataType) ) {
        return RSRV_ERROR;
    }

    pciu = MPTOPCIU ( mp );
    if ( ! pciu ) {
        logBadId ( client, mp, pPayload );
        return RSRV_ERROR;
    }

    /*
     * stop further use of server if memory becomes scarce
     */
    spaceAvailOnFreeList = freeListItemsAvail ( rsrvEventFreeList ) > 0;
    if ( osiSufficentSpaceInPool(sizeof(*pevext)) || spaceAvailOnFreeList ) {
        pevext = (struct event_ext *) freeListCalloc (rsrvEventFreeList);
    }
    else {
        pevext = 0;
    }

    if (!pevext) {
        log_header ("no memory to add subscription",
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err(
            mp,
            ECA_ALLOCMEM,
            client,
            RECORD_NAME(pciu->dbch));
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    pevext->msg = *mp;
    pevext->pciu = pciu;
    pevext->size = dbr_size_n(mp->m_dataType, mp->m_count);
    pevext->mask = ntohs ( pmi->m_mask );

    epicsMutexMustLock(client->eventqLock);
    ellAdd( &pciu->eventq, &pevext->node);
    epicsMutexUnlock(client->eventqLock);

    pevext->pdbev = db_add_event (client->evuser, pciu->dbch,
                read_reply, pevext, pevext->mask);
    if (pevext->pdbev == NULL) {
        log_header ("no memory to add subscription to db",
            client, mp, pPayload, 0);
        SEND_LOCK(client);
        send_err (mp, ECA_ALLOCMEM, client,
            "subscription install into record %s failed",
            RECORD_NAME(pciu->dbch));
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }

    /*
     * always send it once at event add
     */
    /*
     * if the client program issues many monitors
     * in a row then I recv when the send side
     * of the socket would block. This prevents
     * a application program initiated deadlock.
     *
     * However when I am reconnecting I reissue
     * the monitors and I could get deadlocked.
     * The client is blocked sending and the server
     * task for the client is blocked sending in
     * this case. I cant check the recv part of the
     * socket in the client since I am still handling an
     * outstanding recv ( they must be processed in order).
     * I handle this problem in the server by using
     * post_single_event() below instead of calling
     * read_reply() in this module. This is a complete
     * fix since a monitor setup is the only request
     * soliciting a reply in the client which is
     * issued from inside of service.c (from inside
     * of the part of the ca client which services
     * messages sent by the server).
     */

    DLOG ( 3, ("event_add_action: db_post_single_event (0x%X)\n",
        pevext->pdbev) );
    db_post_single_event(pevext->pdbev);

    /*
     * enable future labor if we have read access
     */
    if(asCheckGet(pciu->asClientPVT)){
        db_event_enable(pevext->pdbev);
    }
    else {
        DLOG ( 3, ( "Disable event because cannot read\n" ) );
    }

    return RSRV_OK;
}

/*
 *  clear_channel_reply()
 */
static int clear_channel_reply ( caHdrLargeArray *mp,
        void *pPayload, struct client  *client )
{
     struct event_ext *pevext;
     struct channel_in_use *pciu;
     int status;

     /*
      *
      * Verify the channel
      *
      */
     pciu = MPTOPCIU(mp);
     if(pciu?pciu->client!=client:TRUE){
         logBadId ( client, mp, pPayload );
         return RSRV_ERROR;
     }

     rsrvFreePutNotify ( client, pciu->pPutNotify );

     while (TRUE){
         epicsMutexMustLock(client->eventqLock);
         pevext = (struct event_ext *) ellGet(&pciu->eventq);
         epicsMutexUnlock(client->eventqLock);

         if(!pevext){
             break;
         }

         if (pevext->pdbev) {
             db_cancel_event (pevext->pdbev);
         }
         freeListFree(rsrvEventFreeList, pevext);
     }

     db_flush_extra_labor_event ( client->evuser );

     /*
      * send delete confirmed message
      */
     SEND_LOCK(client);
     status = cas_copy_in_header ( client, CA_PROTO_CLEAR_CHANNEL,
        0u, mp->m_dataType, mp->m_count, mp->m_cid,
        mp->m_available, NULL );
     if ( status != ECA_NORMAL ) {
        SEND_UNLOCK(client);
        return RSRV_ERROR;
     }

     cas_commit_msg ( client, 0u );
     SEND_UNLOCK(client);

     /*
      * remove from access control list
      */
     status = asRemoveClient(&pciu->asClientPVT);
     if(status != 0 && status != S_asLib_asNotActive){
         errMessage(status, RECORD_NAME(pciu->dbch));
         return RSRV_ERROR;
     }
     
     epicsMutexMustLock ( client->chanListLock );
     if ( pciu->state == rsrvCS_inService ||
            pciu->state == rsrvCS_pendConnectResp  ) {
        ellDelete ( &client->chanList, &pciu->node );
        pciu->state = rsrvCS_shutdown;
     }
     else if ( pciu->state == rsrvCS_inServiceUpdatePendAR ||
            pciu->state == rsrvCS_pendConnectRespUpdatePendAR ) {
        ellDelete ( &client->chanPendingUpdateARList, &pciu->node );
        pciu->state = rsrvCS_shutdown;
     }
     else if ( pciu->state == rsrvCS_shutdown ) {
         /* no-op */
     }
     else {
        epicsMutexUnlock( client->chanListLock );
        SEND_LOCK(client);
        send_err(mp, ECA_INTERNAL, client,
            "channel was in strange state or corrupted during cleanup");
        SEND_UNLOCK(client);
        return RSRV_ERROR;
     }
     epicsMutexUnlock( client->chanListLock );

     LOCK_CLIENTQ;
     status = bucketRemoveItemUnsignedId (pCaBucket, &pciu->sid);
     if(status != S_bucket_success){
         UNLOCK_CLIENTQ;
         errMessage (status, "Bad resource id during channel clear");
         logBadId ( client, mp, pPayload );
         return RSRV_ERROR;
     }
     rsrvChannelCount--;
     UNLOCK_CLIENTQ;

     dbChannelDelete(pciu->dbch);
     freeListFree(rsrvChanFreeList, pciu);

     return RSRV_OK;
}

/*
 *
 *  event_cancel_reply()
 *
 *
 * Much more efficient now since the event blocks hang off the channel in use
 * blocks not all together off the client block.
 */
static int event_cancel_reply ( caHdrLargeArray *mp, void *pPayload, struct client *client )
{
     struct channel_in_use  *pciu;
     struct event_ext       *pevext;
     int                    status;

     /*
      *
      * Verify the channel
      *
      */
     pciu = MPTOPCIU(mp);
     if (pciu?pciu->client!=client:TRUE) {
         logBadId ( client, mp, pPayload );
         return RSRV_ERROR;
     }

     /*
      * search events on this channel for a match
      * (there are usually very few monitors per channel)
      */
     epicsMutexMustLock(client->eventqLock);
     for (pevext = (struct event_ext *) ellFirst(&pciu->eventq);
            pevext; pevext = (struct event_ext *) ellNext(&pevext->node)){

         if (pevext->msg.m_available == mp->m_available) {
             ellDelete(&pciu->eventq, &pevext->node);
             break;
         }
     }
     epicsMutexUnlock(client->eventqLock);

     /*
      * Not Found- return an exception event
      */
     if(!pevext){
         SEND_LOCK(client);
         send_err(mp, ECA_BADMONID, client, RECORD_NAME(pciu->dbch));
         SEND_UNLOCK(client);
         return RSRV_ERROR;
     }

     /*
      * cancel monitor activity in progress
      */
     if (pevext->pdbev) {
         db_cancel_event (pevext->pdbev);
     }

     /*
      * send delete confirmed message
      */
     SEND_LOCK(client);

     status = cas_copy_in_header ( client, pevext->msg.m_cmmd,
        0u, pevext->msg.m_dataType, pevext->msg.m_count, pevext->msg.m_cid,
        pevext->msg.m_available, NULL );
     if ( status != ECA_NORMAL ) {
         SEND_UNLOCK(client);
         return RSRV_ERROR;
     }
     cas_commit_msg ( client, 0 );
     SEND_UNLOCK(client);

     freeListFree (rsrvEventFreeList, pevext);

     return RSRV_OK;
}

/*
 *  read_sync_reply()
 */
static int read_sync_reply ( caHdrLargeArray *mp, void *pPayload, struct client *client )
{
    int status;
    SEND_LOCK(client);
    status = cas_copy_in_header ( client, mp->m_cmmd,
        0u, mp->m_dataType, mp->m_count, mp->m_cid,
        mp->m_available, NULL );
    if ( status != ECA_NORMAL ) {
        SEND_UNLOCK(client);
        return RSRV_ERROR;
    }
    cas_commit_msg ( client, 0 );
    SEND_UNLOCK(client);
    return RSRV_OK;
}

/*
 *  search_fail_reply()
 *
 *  Only when requested by the client
 *  send search failed reply
 */
static void search_fail_reply ( caHdrLargeArray *mp, void *pPayload, struct client *client)
{
    int status;
    SEND_LOCK ( client );
    status = cas_copy_in_header ( client, CA_PROTO_NOT_FOUND,
        0u, mp->m_dataType, mp->m_count, mp->m_cid, mp->m_available, NULL );
    if ( status != ECA_NORMAL ) {
        SEND_UNLOCK ( client );
        errlogPrintf ( "%s at %d: should always get sufficent space for search fail reply?\n",
            __FILE__, __LINE__ );
        return;
    }
    cas_commit_msg ( client, 0 );
    SEND_UNLOCK ( client );
}

/*
 * udp_version_action()
 */
static int udp_version_action ( caHdrLargeArray *mp, void *pPayload, struct client *client )
{
    client->minor_version_number = mp->m_count;

    if (!CA_VSUPPORTED(mp->m_count)) {
        DLOG ( 2, ( "CAS: Ignore version from unsupported client %u\n", mp->m_count ) );
        return RSRV_ERROR;
    }

    if ( CA_V411 ( mp->m_count ) ) {
        client->seqNoOfReq = mp->m_cid;
    }
    else {
        client->seqNoOfReq = 0;
    }
    return RSRV_OK;
}

/*
 *  rsrv_version_reply()
 */
int rsrv_version_reply ( struct client *client )
{
    int status;
    SEND_LOCK ( client );
    /*
     * sequence number is specified zero when we copy in the
     * header because we dont know it until we receive a datagram
     * from the client
     */
    status = cas_copy_in_header ( client, CA_PROTO_VERSION,
        0, 0, CA_MINOR_PROTOCOL_REVISION,
        0, 0, 0 );
    if ( status != ECA_NORMAL ) {
        SEND_UNLOCK ( client );
        return RSRV_ERROR;
    }
    cas_commit_msg ( client, 0 );
    SEND_UNLOCK ( client );
    return RSRV_OK;
}

/*
 *  search_reply_udp ()
 */
static int search_reply_udp ( caHdrLargeArray *mp, void *pPayload, struct client *client )
{
    ca_uint16_t     *pMinorVersion;
    char            *pName = (char *) pPayload;
    int             status;
    unsigned        sid;
    ca_uint16_t     count;
    ca_uint16_t     type;
    int             spaceAvailOnFreeList;
    size_t          spaceNeeded;
    size_t          reasonableMonitorSpace = 10;

    if (!CA_VSUPPORTED(mp->m_count)) {
        DLOG ( 2, ( "CAS: Ignore search from unsupported client %u\n", mp->m_count ) );
        return RSRV_ERROR;
    }

    /*
     * check the sanity of the message
     */
    if (mp->m_postsize<=1) {
        log_header ("empty PV name in UDP search request?",
            client, mp, pPayload, 0);
        return RSRV_OK;
    }
    pName[mp->m_postsize-1] = '\0';

    /* Exit quickly if channel not on this node */
    if (dbChannelTest(pName)) {
        DLOG ( 2, ( "CAS: Lookup for channel \"%s\" failed\n", pPayLoad ) );
        return RSRV_OK;
    }

    /*
     * stop further use of server if memory becomes scarce
     */
    spaceAvailOnFreeList =     freeListItemsAvail ( rsrvChanFreeList ) > 0
                            && freeListItemsAvail ( rsrvEventFreeList ) > reasonableMonitorSpace;
    spaceNeeded = sizeof (struct channel_in_use) +
        reasonableMonitorSpace * sizeof (struct event_ext);
    if ( ! ( osiSufficentSpaceInPool(spaceNeeded) || spaceAvailOnFreeList ) ) {
        return RSRV_ERROR;
    }

    /*
     * starting with V4.4 the count field is used (abused)
     * to store the minor version number of the client.
     *
     * New versions dont alloc the channel in response
     * to a search request.
     * For these, allocation has been moved to claim_ciu_action().
     *
     * m_count, m_cid are already in host format...
     */
    if (CA_V44(mp->m_count)) {
        sid = ~0U;
        count = 0;
        type = ca_server_port;
    }
    else {
        /* shouldn't actually get here due to VSUPPORTED test */
        return RSRV_ERROR;
    }

    SEND_LOCK ( client );
    status = cas_copy_in_header ( client, CA_PROTO_SEARCH,
        sizeof(*pMinorVersion), type, count,
        sid, mp->m_available,
        ( void * ) &pMinorVersion );
    if ( status != ECA_NORMAL ) {
        SEND_UNLOCK ( client );
        return RSRV_ERROR;
    }

    /*
     * Starting with CA V4.1 the minor version number
     * is appended to the end of each search reply.
     * This value is ignored by earlier clients.
     */
    *pMinorVersion = htons ( CA_MINOR_PROTOCOL_REVISION );

    cas_commit_msg ( client, sizeof ( *pMinorVersion ) );
    SEND_UNLOCK ( client );

    return RSRV_OK;
}

/*
 *  search_reply_tcp ()
 */
static int search_reply_tcp ( 
    caHdrLargeArray *mp, void *pPayload, struct client *client )
{
    char            *pName = (char *) pPayload;
    int             status;
    int             spaceAvailOnFreeList;
    size_t          spaceNeeded;
    size_t          reasonableMonitorSpace = 10;

    if (!CA_VSUPPORTED(mp->m_count)) {
        DLOG ( 2, ( "CAS: Ignore search from unsupported client %u\n", mp->m_count ) );
        return RSRV_ERROR;
    }

    /*
     * check the sanity of the message
     */
    if (mp->m_postsize<=1) {
        log_header ("empty PV name in UDP search request?", 
            client, mp, pPayload, 0);
        return RSRV_OK;
    }
    pName[mp->m_postsize-1] = '\0';

    /* Exit quickly if channel not on this node */
    if (dbChannelTest(pName)) {
        DLOG ( 2, ( "CAS: Lookup for channel \"%s\" failed\n", pPayLoad ) );
        if (mp->m_dataType == DOREPLY)
            search_fail_reply ( mp, pPayload, client );
        return RSRV_OK;
    }

    /*
     * stop further use of server if memory becomes scarse
     */
    spaceAvailOnFreeList =     freeListItemsAvail ( rsrvChanFreeList ) > 0
                            && freeListItemsAvail ( rsrvEventFreeList ) > reasonableMonitorSpace;
    spaceNeeded = sizeof (struct channel_in_use) + 
        reasonableMonitorSpace * sizeof (struct event_ext);
    if ( ! ( osiSufficentSpaceInPool(spaceNeeded) || spaceAvailOnFreeList ) ) { 
        SEND_LOCK(client);
        send_err ( mp, ECA_ALLOCMEM, client, "Server memory exhausted" );
        SEND_UNLOCK(client);
        return RSRV_OK;
    }

    SEND_LOCK ( client );
    status = cas_copy_in_header ( client, CA_PROTO_SEARCH, 
        0, ca_server_port, 0, ~0U, mp->m_available, 0 );
    if ( status != ECA_NORMAL ) {
        SEND_UNLOCK ( client );
        return RSRV_ERROR;
    }

    cas_commit_msg ( client, 0 );
    SEND_UNLOCK ( client );

    return RSRV_OK;
}

typedef int (*pProtoStubTCP) (caHdrLargeArray *mp, void *pPayload, struct client *client);

/*
 * TCP protocol jump table
 */
static const pProtoStubTCP tcpJumpTable[] =
{
    tcp_version_action,
    event_add_action,
    event_cancel_reply,
    read_action,
    write_action,
    bad_tcp_cmd_action,
    search_reply_tcp,
    bad_tcp_cmd_action,
    events_off_action,
    events_on_action,
    read_sync_reply,
    bad_tcp_cmd_action,
    clear_channel_reply,
    bad_tcp_cmd_action,
    bad_tcp_cmd_action,
    read_notify_action,
    bad_tcp_cmd_action,
    bad_tcp_cmd_action,
    claim_ciu_action,
    write_notify_action,
    client_name_action,
    host_name_action,
    bad_tcp_cmd_action,
    tcp_echo_action,
    bad_tcp_cmd_action,
    bad_tcp_cmd_action,
    bad_tcp_cmd_action,
    bad_tcp_cmd_action
};

/*
 * UDP protocol jump table
 */
typedef int (*pProtoStubUDP) (caHdrLargeArray *mp, void *pPayload, struct client *client);
static const pProtoStubUDP udpJumpTable[] =
{
    udp_version_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    search_reply_udp,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action,
    bad_udp_cmd_action
};

/*
 * CAMESSAGE()
 */
int camessage ( struct client *client )
{
    unsigned nmsg = 0;
    unsigned msgsize;
    unsigned bytes_left;
    int status = RSRV_ERROR;

    assert(pCaBucket);

    /* drain remnents of large messages that will not fit */
    if ( client->recvBytesToDrain ) {
        if ( client->recvBytesToDrain >= client->recv.cnt ) {
            client->recvBytesToDrain -= client->recv.cnt;
            client->recv.stk = client->recv.cnt;
            return RSRV_OK;
        }
        else {
            client->recv.stk += client->recvBytesToDrain;
            client->recvBytesToDrain = 0u;
        }
    }

    DLOG ( 2, ( "CAS: Parsing %d(decimal) bytes\n", recv->cnt ) );

    while ( 1 )
    {
        caHdrLargeArray msg;
        caHdr *mp;
        void *pBody;

        /* wait for at least a complete caHdr */
        bytes_left = client->recv.cnt - client->recv.stk;
        if ( bytes_left < sizeof(*mp) ) {
            status = RSRV_OK;
            break;
        }

        mp = (caHdr *) &client->recv.buf[client->recv.stk];
        msg.m_cmmd      = ntohs ( mp->m_cmmd );
        msg.m_postsize  = ntohs ( mp->m_postsize );
        msg.m_dataType  = ntohs ( mp->m_dataType );
        msg.m_count     = ntohs ( mp->m_count );
        msg.m_cid       = ntohl ( mp->m_cid );
        msg.m_available = ntohl ( mp->m_available );

        if ( CA_V49(client->minor_version_number) && msg.m_postsize == 0xffff  ) {
            ca_uint32_t *pLW = ( ca_uint32_t * ) ( mp + 1 );
            if ( bytes_left < sizeof(*mp) + 2 * sizeof(*pLW) ) {
                status = RSRV_OK;
                break;
            }
            msg.m_postsize  = ntohl ( pLW[0] );
            msg.m_count     = ntohl ( pLW[1] );
            msgsize = msg.m_postsize + sizeof(*mp) + 2 * sizeof ( *pLW );
            pBody = ( void * ) ( pLW + 2 );
        }
        else {
            msgsize = msg.m_postsize + sizeof(*mp);
            pBody = ( void * ) ( mp + 1 );
        }

        /* ignore deprecated clients, but let newer clients identify themselves. */
        if (msg.m_cmmd!=CA_PROTO_VERSION && !CA_VSUPPORTED(client->minor_version_number)) {
            if (client->proto==IPPROTO_TCP) {
                /* log and error for too old clients, but keep the connection open to avoid a
                 * re-connect loop.
                 */
                send_err ( &msg, ECA_DEFUNCT, client,
                    "CAS: Client version %u too old", client->minor_version_number );
                log_header ( "CAS: Client version too old",
                    client, &msg, 0, nmsg );
                client->recvBytesToDrain = msgsize - bytes_left;
                client->recv.stk = client->recv.cnt;
                status = RSRV_OK;
            } else {
                /* silently ignore UDP from old clients */
                status = RSRV_ERROR;
            }
            break;
        }

        /*
         * disconnect clients that dont send 8 byte
         * aligned payloads
         */
        if ( msgsize & 0x7 ) {
            if (client->proto==IPPROTO_TCP) {
                send_err ( &msg, ECA_INTERNAL, client,
                    "CAS: Missaligned protocol rejected" );
                log_header ( "CAS: Missaligned protocol rejected",
                    client, &msg, 0, nmsg );
            }
            status = RSRV_ERROR;
            break;
        }

        /* problem: we have a complete header,
         * but before we check msgsize we don't know
         * if we have a complete message body
         * -> we may be called again with the same header
         *    after receiving the full message
         */
        if ( msgsize > client->recv.maxstk ) {
            casExpandRecvBuffer ( client, msgsize );
            if ( msgsize > client->recv.maxstk ) {
                if (client->proto==IPPROTO_TCP) {
                    send_err ( &msg, ECA_TOLARGE, client,
                        "CAS: Server unable to load large request message. Max bytes=%lu",
                        rsrvSizeofLargeBufTCP );
                    log_header ( "CAS: server unable to load large request message",
                        client, &msg, 0, nmsg );
                }
                assert ( client->recv.cnt <= client->recv.maxstk );
                assert ( msgsize >= bytes_left );
                client->recvBytesToDrain = msgsize - bytes_left;
                client->recv.stk = client->recv.cnt;
                status = RSRV_OK;
                break;
            }
        }

        /*
         * wait for complete message body
         */
        if ( msgsize > bytes_left ) {
            status = RSRV_OK;
            break;
        }

        nmsg++;

        if ( CASDEBUG > 2 )
            log_header (NULL, client, &msg, pBody, nmsg);

        if ( client->proto==IPPROTO_UDP ) {
            if ( msg.m_cmmd < NELEMENTS ( udpJumpTable ) ) {
                status = ( *udpJumpTable[msg.m_cmmd] )( &msg, pBody, client );
                if (status!=RSRV_OK) {
                    status = RSRV_ERROR;
                    break;
                }
            }
            else {
                status = bad_udp_cmd_action ( &msg, pBody, client );
                break;
            }
        }
        else {
            if ( msg.m_cmmd < NELEMENTS(tcpJumpTable) ) {
                status = ( *tcpJumpTable[msg.m_cmmd] ) ( &msg, pBody, client );
                if ( status != RSRV_OK ) {
                    status = RSRV_ERROR;
                    break;
                }
            }
            else {
                return bad_tcp_cmd_action ( &msg, pBody, client );
            }
        }

        client->recv.stk += msgsize;
    }

    return status;
}

/*
 * rsrvCheckPut ()
 */
int rsrvCheckPut (const struct channel_in_use *pciu)
{
    /*
     * SPC_NOMOD fields are always unwritable
     */
    if (dbChannelSpecial(pciu->dbch) == SPC_NOMOD) {
        return 0;
    }
    else {
        return asCheckPut (pciu->asClientPVT);
    }
}
