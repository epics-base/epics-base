
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 *
 * Notes:
 * 1) when rdix is equal to wtix it indicates that the entire buffer is
 * available to be read, and therefore nothing can be written.
 * 2) the byte at index rdix + 1 is the next byte to read.
 * 3) the byte at index wtix is the next byte to write.
 */

#include <string.h>

#include "ringBuffer.h"

static const unsigned ringIndexMask = nElementsInRing - 1;

/*
 * cacRingBufferConstruct ()
 */
bool cacRingBufferConstruct ( ringBuffer *pBuf )
{
    pBuf->rdix = 0u;
    pBuf->wtix = 1u;

    pBuf->readLock = semMutexCreate ();
    if ( ! pBuf->readLock ) {
        return false;
    }

    pBuf->writeLock = semMutexCreate ();
    if ( ! pBuf->writeLock ) {
        semMutexDestroy ( pBuf->readLock );
        return false;
    }

    return true;
}

/*
 * cacRingBufferDestroy ()
 */
void cacRingBufferDestroy (ringBuffer *pBuf)
{
    semMutexDestroy ( pBuf->readLock );
    semMutexDestroy ( pBuf->writeLock );
}

/*
 * cacRingBufferReadSize ()
 */
static inline unsigned cacRingBufferReadSize ( ringBuffer *pBuf )
{
    unsigned long   count;
    
    if ( pBuf->wtix <= pBuf->rdix ) {
        static const unsigned bufSizeM1 = sizeof ( pBuf->buf ) - 1u;
        count = ( bufSizeM1 - pBuf->rdix ) + pBuf->wtix;
    }
    else  {
        count = ( pBuf->wtix - pBuf->rdix ) - 1u;
    }
    return count;
}

/*
 * cacRingBufferContiguousReadSize ()
 */
static inline unsigned cacRingBufferContiguousReadSize (ringBuffer *pBuf)
{
    static const unsigned bufSizeM1 = sizeof ( pBuf->buf ) - 1u;
    unsigned long count;
    
    if ( pBuf->wtix <= pBuf->rdix ) {
        if ( pBuf->rdix == bufSizeM1 ) {
            count = pBuf->wtix;
        }
        else {
            count = bufSizeM1 - pBuf->rdix;
        }
    }
    else {
        count = ( pBuf->wtix - pBuf->rdix ) - 1u;
    }
    return count;
}

/*
 * cacRingBufferWriteSize ()
 */
static inline unsigned cacRingBufferWriteSize ( ringBuffer *pBuf )
{
    unsigned long   count;

    if ( pBuf->wtix <= pBuf->rdix ) {
        count = pBuf->rdix - pBuf->wtix;
    }
    else {
        count = ( sizeof (pBuf->buf) - pBuf->wtix ) + pBuf->rdix;
    }
    return count;
}

/*
 * cacRingBufferContiguousWriteSize ()
 */
static inline unsigned cacRingBufferContiguousWriteSize ( ringBuffer *pBuf )
{
    unsigned long   count;

    if ( pBuf->wtix <= pBuf->rdix ) {
        count = pBuf->rdix - pBuf->wtix;
    }
    else {
        count = sizeof ( pBuf->buf ) - pBuf->wtix;
    }
    return count;
}


/*
 * cacRingBufferReadPartial ()
 *
 * returns the number of bytes read which may be less than
 * the number requested.
 */
static unsigned cacRingBufferReadPartial ( ringBuffer *pRing, void *pBuf,
                unsigned nBytes )
{
    unsigned totalBytes;

    if ( pRing->wtix <= pRing->rdix ) {
        static const unsigned bufSizeM1 = sizeof ( pRing->buf ) - 1u;
        unsigned nBytesAvail1stBlock, nBytesAvail2ndBlock;

        if ( pRing->rdix == bufSizeM1 ) {
            nBytesAvail1stBlock = pRing->wtix;
            nBytesAvail2ndBlock = 0u;
        }
        else {
            nBytesAvail1stBlock = bufSizeM1 - pRing->rdix;
            nBytesAvail2ndBlock = pRing->wtix;
        }
        if ( nBytesAvail1stBlock >= nBytes ) {
            totalBytes = nBytes;
            memcpy ( pBuf, pRing->buf + pRing->rdix + 1u, totalBytes );
        }
        else {
            char *pChar = (char *) pBuf;
            memcpy ( pBuf, pRing->buf + pRing->rdix + 1u, nBytesAvail1stBlock );
            nBytes -= nBytesAvail1stBlock;
            if ( nBytesAvail2ndBlock > nBytes ) {
                nBytesAvail2ndBlock = nBytes;
            }
            memcpy ( pChar + nBytesAvail1stBlock, pRing->buf, nBytesAvail2ndBlock);
            totalBytes = nBytesAvail1stBlock + nBytesAvail2ndBlock;
        }
        pRing->rdix += totalBytes;
        pRing->rdix &= ringIndexMask;
    }
    else {
        totalBytes = ( pRing->wtix - pRing->rdix ) - 1;
        if ( totalBytes > nBytes ) {
            totalBytes = nBytes;
        }
        memcpy ( pBuf, pRing->buf+pRing->rdix+1, totalBytes );
        pRing->rdix += totalBytes;
        pRing->rdix &= ringIndexMask;
    }

    return totalBytes;
}

/*
 * cacRingBufferWritePartial ()
 *
 * returns the number of bytes written which may be less than
 * the number requested.
 */
static unsigned cacRingBufferWritePartial ( ringBuffer *pRing, 
            const void *pBuf, unsigned nBytes )
{
    unsigned totalBytes;

    if ( pRing->wtix < pRing->rdix ) {
        totalBytes = pRing->rdix - pRing->wtix;
        if ( totalBytes > nBytes ) {
            totalBytes = nBytes;
        }
        memcpy (pRing->buf+pRing->wtix, pBuf, totalBytes);
        pRing->wtix += totalBytes;
        pRing->wtix &= ringIndexMask;
    }
    else if ( pRing->wtix > pRing->rdix ) {
        unsigned nBytesAvail1stBlock, nBytesAvail2ndBlock;

        nBytesAvail1stBlock = sizeof (pRing->buf) - pRing->wtix;
        nBytesAvail2ndBlock = pRing->rdix;
        if ( nBytesAvail1stBlock >= nBytes ) {
            totalBytes = nBytes;
            memcpy ( pRing->buf+pRing->wtix, pBuf, totalBytes );
        }
        else {
            char *pChar = (char *) pBuf;
            memcpy ( pRing->buf+pRing->wtix, pBuf, nBytesAvail1stBlock );
            nBytes -= nBytesAvail1stBlock;
            if ( nBytesAvail2ndBlock > nBytes ) {
                nBytesAvail2ndBlock = nBytes;
            }
            memcpy (pRing->buf, pChar+nBytesAvail1stBlock, 
                                    nBytesAvail2ndBlock);
            totalBytes = nBytesAvail2ndBlock + nBytesAvail1stBlock;
        }
        pRing->wtix += totalBytes;
        pRing->wtix &= ringIndexMask;
    }
    else {
        totalBytes = 0;
    }

    return totalBytes;
}

void cacRingBufferWriteLock ( ringBuffer *pBuf )
{
    semMutexMustTake ( pBuf->writeLock );
}

bool cacRingBufferWriteLockIfBytesAvailable ( ringBuffer *pBuf, unsigned bytesRequired )
{
    semMutexMustTake ( pBuf->writeLock );
    if ( cacRingBufferWriteSize (pBuf) < bytesRequired ) {
        semMutexGive ( pBuf->writeLock );
        return false;
    }
    return true;
}

void cacRingBufferWriteUnlock ( ringBuffer *pBuf )
{
    semMutexGive ( pBuf->writeLock );
}

void *cacRingBufferWriteReserve ( ringBuffer *pRing, unsigned *pBytesAvail )
{
    unsigned avail;

    semMutexMustTake ( pRing->writeLock );
    
    avail = cacRingBufferContiguousWriteSize ( pRing );

    if ( avail == 0 ) {
        *pBytesAvail = 0u;
        semMutexGive ( pRing->writeLock );
        return NULL;
    }

    *pBytesAvail = avail;

    return (void *) &pRing->buf[pRing->wtix];
}

void cacRingBufferWriteCommit ( ringBuffer *pRing, unsigned delta )
{
    pRing->wtix += delta;
    pRing->wtix &= ringIndexMask;
    semMutexGive ( pRing->writeLock );
}

bool cacRingBufferWriteNoBlock ( ringBuffer *pBuf, const void *pMsg, unsigned bytesRequired )
{
    unsigned nBytesWritten;

    semMutexMustTake ( pBuf->writeLock );
    if ( cacRingBufferWriteSize ( pBuf ) < bytesRequired ) {
        semMutexGive ( pBuf->writeLock );
        return false;
    }

    nBytesWritten = cacRingBufferWritePartial ( pBuf, pMsg, bytesRequired );
    semMutexGive ( pBuf->writeLock );
    return nBytesWritten == bytesRequired;
}

bool cacRingBufferWriteMultipartMessageNoBlock ( ringBuffer *pBuf, 
                     const msgDescriptor *pMsgs, unsigned nMsgs )
{
    unsigned i;
    unsigned totalBytes = 0u;
    unsigned nBytesWritten;

    for ( i = 0u; i < nMsgs; i++ ) {
        totalBytes += pMsgs[i].length;
    }

    semMutexMustTake ( pBuf->writeLock );

    if ( cacRingBufferWriteSize ( pBuf ) < totalBytes ) {
        semMutexGive ( pBuf->writeLock );
        return false;
    }

    for ( i = 0u; i < nMsgs; i++ ) {
        nBytesWritten = cacRingBufferWritePartial ( pBuf, 
                pMsgs[i].pMsg, pMsgs[i].length );
        if ( nBytesWritten != pMsgs[i].length ) {
            semMutexGive ( pBuf->writeLock );
            return false;
        }
    }

    semMutexGive ( pBuf->writeLock );
    return true;
}

unsigned cacRingBufferWrite ( ringBuffer *pBuf, const void *pMsg, unsigned nBytes )
{
    unsigned nBytesWritten;

    semMutexMustTake ( pBuf->writeLock );
    nBytesWritten = cacRingBufferWritePartial ( pBuf, pMsg, nBytes );
    semMutexGive ( pBuf->writeLock );

    return nBytesWritten;
}

void *cacRingBufferReadReserve ( ringBuffer *pRing, unsigned *pBytesAvail )
{
    unsigned avail;

    semMutexMustTake ( pRing->readLock );
    
    avail = cacRingBufferContiguousReadSize ( pRing );

    if ( avail == 0 ) {
        *pBytesAvail = 0u;
        semMutexGive ( pRing->readLock );
        return NULL;
    }

    *pBytesAvail = avail;

    return (void *) &pRing->buf[ ( pRing->rdix + 1 ) & ringIndexMask ];
}


void cacRingBufferReadCommit ( ringBuffer *pRing, unsigned delta )
{
    pRing->rdix += delta;
    pRing->rdix &= ringIndexMask;
    semMutexGive ( pRing->readLock );
}

bool cacRingBufferReadNoBlock ( ringBuffer *pBuf, void *pDest, unsigned nBytesRequired )
{
    semMutexMustTake ( pBuf->readLock );
    unsigned available = cacRingBufferReadSize ( pBuf );
    if ( available < nBytesRequired) {
        semMutexGive ( pBuf->readLock );
        return false;
    }
    char *pCurrent = static_cast <char *> ( pDest );
    unsigned totalBytes = cacRingBufferReadPartial ( pBuf, pCurrent, nBytesRequired );
    unsigned diff = nBytesRequired - totalBytes;
    if ( diff ) {
        totalBytes += cacRingBufferReadPartial ( pBuf, &pCurrent[totalBytes], diff );
        assert ( totalBytes == nBytesRequired );
    }
    semMutexGive ( pBuf->readLock );
    return true;
}

