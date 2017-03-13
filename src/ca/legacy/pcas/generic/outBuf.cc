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
 *              johill@lanl.gov
 *              505 665 1831
 */

#include "errlog.h"
#include "epicsTime.h"

#define epicsExportSharedSymbols
#include "outBuf.h" 
#include "osiWireFormat.h"

const char * outBufClient :: ppFlushCondText[3] = 
{
    "flushNone",
    "flushProgress",
    "flushDisconnect"
};

//
// outBuf::outBuf()
//
outBuf::outBuf ( outBufClient & clientIn, 
                clientBufMemoryManager & memMgrIn ) : 
    client ( clientIn ), memMgr ( memMgrIn ), bufSize ( 0 ), 
        stack ( 0u ), ctxRecursCount ( 0u )
{
    casBufferParm bufParm = memMgr.allocate ( 1 );
    this->pBuf = bufParm.pBuf;
    this->bufSize = bufParm.bufSize;
    memset ( this->pBuf, '\0', this->bufSize );
}

//
// outBuf::~outBuf()
//
outBuf::~outBuf()
{
    assert ( this->ctxRecursCount == 0 );
    memMgr.release ( this->pBuf, this->bufSize );
}

//
// outBuf::allocRawMsg ()
//
caStatus outBuf::allocRawMsg ( bufSizeT msgsize, void **ppMsg )
{
    bufSizeT stackNeeded;

    msgsize = CA_MESSAGE_ALIGN ( msgsize );

    if ( msgsize > this->bufSize ) {
        this->expandBuffer (msgsize);
        if ( msgsize > this->bufSize ) {
            return S_cas_hugeRequest;
        }
    }

    stackNeeded = this->bufSize - msgsize;

    if ( this->stack > stackNeeded ) {
        //
        // Try to flush the output queue
        //
        this->flush ();

        //
        // If this failed then the fd is nonblocking 
        // and we will let select() take care of it
        //
        if ( this->stack > stackNeeded ) {
            this->client.sendBlockSignal();
            return S_cas_sendBlocked;
        }
    }

    //
    // it fits so commitMsg() will move the stack pointer forward
    //
    *ppMsg = (void *) &this->pBuf[this->stack];

    return S_cas_success;
}

// code size is allowed to increase here somewhat in the
// interest of efficency since this is a very frequently
// called function
caStatus outBuf::copyInHeader ( ca_uint16_t response, ca_uint32_t payloadSize,
    ca_uint16_t dataType, ca_uint32_t nElem, ca_uint32_t cid, 
    ca_uint32_t responseSpecific, void **ppPayload )
{
    ca_uint32_t alignedPayloadSize = CA_MESSAGE_ALIGN ( payloadSize );
    char * pPayload;

    if ( alignedPayloadSize < 0xffff && nElem < 0xffff ) {
        ca_uint32_t msgSize = sizeof ( caHdr ) + alignedPayloadSize;
        caHdr * pHdr;
        caStatus status = this->allocRawMsg ( 
            msgSize, reinterpret_cast < void ** > ( & pHdr ) );
        if ( status != S_cas_success ) {
            return status;
        }

        AlignedWireRef < epicsUInt16 > ( pHdr->m_cmmd ) = response;
        AlignedWireRef < epicsUInt16 > ( pHdr->m_dataType ) = dataType;
        AlignedWireRef < epicsUInt32 > ( pHdr->m_cid ) = cid;
        AlignedWireRef < epicsUInt32 > ( pHdr->m_available ) = responseSpecific;
        AlignedWireRef < epicsUInt16 > ( pHdr->m_postsize ) = 
            static_cast < epicsUInt16 > ( alignedPayloadSize );
        AlignedWireRef < epicsUInt16 > ( pHdr->m_count ) = 
            static_cast < epicsUInt16 > ( nElem );
        pPayload = reinterpret_cast < char * > ( pHdr + 1 );
    }
    else {
        ca_uint32_t msgSize = sizeof ( caHdr ) + 
            2 * sizeof (ca_uint32_t) + alignedPayloadSize;
        caHdr * pHdr;
        caStatus status = this->allocRawMsg ( 
            msgSize, reinterpret_cast < void ** > ( & pHdr ) );
        if ( status != S_cas_success ) {
            return status;
        }

        AlignedWireRef < epicsUInt16 > ( pHdr->m_cmmd ) = response;
        AlignedWireRef < epicsUInt16 > ( pHdr->m_dataType ) = dataType;
        AlignedWireRef < epicsUInt32 > ( pHdr->m_cid ) = cid;
        AlignedWireRef < epicsUInt32 > ( pHdr->m_available ) = responseSpecific;
        AlignedWireRef < epicsUInt16 > ( pHdr->m_postsize ) = 0xffff;
        AlignedWireRef < epicsUInt16 > ( pHdr->m_count ) = 0;
        ca_uint32_t * pLW = reinterpret_cast < ca_uint32_t * > ( pHdr + 1 );
        AlignedWireRef < epicsUInt32 > sizeWireRef ( pLW[0] );
        sizeWireRef = alignedPayloadSize;
        AlignedWireRef < epicsUInt32 > nElemWireRef ( pLW[1] );
        nElemWireRef= nElem;
        pPayload = reinterpret_cast < char * > ( pLW + 2 );
    }

    /* zero out pad bytes */
    if ( alignedPayloadSize > payloadSize ) {
        memset ( pPayload + payloadSize, '\0', 
                 alignedPayloadSize - payloadSize );
    }

    if ( ppPayload ) {
        *ppPayload = pPayload;
    }

    return S_cas_success;
}

//
// outBuf::commitMsg ()
//
void outBuf::commitMsg ()
{
    ca_uint32_t payloadSize;
    ca_uint32_t elementCount;
    ca_uint32_t hdrSize;

    const caHdr * mp = ( caHdr * ) & this->pBuf[ this->stack ];
    if ( mp->m_postsize == 0xffff || mp->m_count == 0xffff ) {
        const ca_uint32_t *pLW = reinterpret_cast <const ca_uint32_t *> ( mp + 1 );
        payloadSize = AlignedWireRef < const epicsUInt32 > ( pLW[0] );
        elementCount = AlignedWireRef < const epicsUInt32 > ( pLW[1] );
        hdrSize = sizeof ( caHdr ) + 2 * sizeof ( ca_uint32_t );
    }
    else {
        payloadSize = AlignedWireRef < const epicsUInt16 > ( mp->m_postsize );
        elementCount = AlignedWireRef < const epicsUInt16 > ( mp->m_count );
        hdrSize = sizeof ( caHdr );
    }

    this->commitRawMsg ( hdrSize + payloadSize );

    unsigned debugLevel = this->client.getDebugLevel();
    if ( debugLevel ) {
        epicsUInt16 cmmd = AlignedWireRef < const epicsUInt16 > ( mp->m_cmmd );
        if ( cmmd != CA_PROTO_VERSION || debugLevel > 2 ) {
            epicsUInt16 type = AlignedWireRef < const epicsUInt16 > ( mp->m_dataType );
            epicsUInt32 cid = AlignedWireRef < const epicsUInt32 > ( mp->m_cid );
            epicsUInt32 avail = AlignedWireRef < const epicsUInt32 > ( mp->m_available );
            fprintf ( stderr,
                "CAS Response: cmd=%d id=%x typ=%d cnt=%d psz=%d avail=%x outBuf ptr=%p \n",
                cmmd, cid, type, elementCount, payloadSize, avail, 
                static_cast < const void * > ( mp ) );
        }
    }
}

//
// outBuf::commitMsg ()
//
void outBuf::commitMsg ( ca_uint32_t reducedPayloadSize )
{
    caHdr * mp = ( caHdr * ) & this->pBuf[ this->stack ];
    reducedPayloadSize = CA_MESSAGE_ALIGN ( reducedPayloadSize );
    if ( mp->m_postsize == 0xffff || mp->m_count == 0xffff ) {
        ca_uint32_t *pLW = reinterpret_cast <ca_uint32_t *> ( mp + 1 );
        AlignedWireRef < epicsUInt32 > payloadSizeExtended ( pLW[0] );
        assert ( reducedPayloadSize <= payloadSizeExtended );
        payloadSizeExtended = reducedPayloadSize;
    }
    else {
        AlignedWireRef < epicsUInt16 > payloadSizeOnWire ( mp->m_postsize );
        ca_uint32_t payloadSize = payloadSizeOnWire;
        assert ( reducedPayloadSize <= payloadSize );
        payloadSizeOnWire = static_cast < ca_uint16_t > ( reducedPayloadSize );
    }
    this->commitMsg ();
}

//
// outBuf::flush ()
//
outBufClient::flushCondition outBuf :: flush ()
{
    if ( this->ctxRecursCount > 0 ) {
        return outBufClient::flushNone;
    }

    bufSizeT nBytesSent;
    //epicsTime beg = epicsTime::getCurrent ();
    outBufClient :: flushCondition cond = 
        this->client.xSend ( this->pBuf, this->stack, nBytesSent );
    //epicsTime end = epicsTime::getCurrent ();
    //printf ( "send of %u bytes, stat =%s, cost us %f u sec\n", 
    //    this->stack, this->client.ppFlushCondText[cond], ( end - beg ) * 1e6 );
    if ( cond == outBufClient::flushProgress ) {
        if ( nBytesSent >= this->stack ) {
            this->stack = 0u;	
        }
        else {
            bufSizeT len = this->stack - nBytesSent;
            //
            // memmove() is ok with overlapping buffers
            //
            //epicsTime beg = epicsTime::getCurrent ();
            memmove ( this->pBuf, &this->pBuf[nBytesSent], len );
            //epicsTime end = epicsTime::getCurrent ();
            //printf ( "mem move cost us %f nano sec\n", ( end - beg ) * 1e9 );
            this->stack = len;
        }

        if ( this->client.getDebugLevel () > 2u ) {
            char buf[64];
            this->client.hostName ( buf, sizeof ( buf ) );
            fprintf ( stderr, "CAS outgoing: %u byte reply to %s\n",
                           nBytesSent, buf );
        }
    }
    return cond;
}

//
// outBuf::pushCtx ()
//
const outBufCtx outBuf::pushCtx ( bufSizeT headerSize,
                                 bufSizeT maxBodySize,
                                 void *&pHeader ) 
{
    bufSizeT totalSize = headerSize + maxBodySize;
    caStatus status;

    status = this->allocRawMsg ( totalSize, & pHeader );
    if ( status != S_cas_success ) {
        return outBufCtx ();
    }
    else if ( this->ctxRecursCount >= UINT_MAX ) {
        return outBufCtx ();
    }
    else {
        outBufCtx result ( *this );
        this->pBuf = this->pBuf + this->stack + headerSize;
        this->stack = 0;
        this->bufSize = maxBodySize;
        this->ctxRecursCount++;
        return result;
    }
}

//
// outBuf::popCtx ()
//
bufSizeT outBuf::popCtx (const outBufCtx &ctx)
{
    if (ctx.stat==outBufCtx::pushCtxSuccess) {
        bufSizeT bytesAdded = this->stack;
        this->pBuf = ctx.pBuf;
        this->bufSize = ctx.bufSize;
        this->stack = ctx.stack;
        assert (this->ctxRecursCount>0u);
        this->ctxRecursCount--;
        return bytesAdded;
    }
    else {
        return 0;
    }
}

//
// outBuf::show (unsigned level)
//
void outBuf::show (unsigned level) const
{
    if ( level > 1u ) {
        printf("\tUndelivered response bytes = %d\n", this->bytesPresent());
    }
}

void outBuf::expandBuffer (bufSizeT needed)
{
    if (needed > bufSize) {
        casBufferParm bufParm;
        try {
            bufParm = this->memMgr.allocate ( needed );
        } catch (std::bad_alloc& e) {
            // caller must check that buffer size has expended
            return;
        }

        memcpy ( bufParm.pBuf, this->pBuf, this->stack );
        this->memMgr.release ( this->pBuf, this->bufSize );
        this->pBuf = bufParm.pBuf;
        this->bufSize = bufParm.bufSize;
    }
}


