
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
 */

#include <string.h>

#include "ringBuffer.h"

static const unsigned ringIndexMask = nElementsInRing-1;

/*
 * cacRingBufferConstruct ()
 */
int cacRingBufferConstruct (ringBuffer *pBuf)
{
    pBuf->shutDown = 0u;
    pBuf->rdix = 0u;
    pBuf->wtix = 1u;

    pBuf->readSignal = semBinaryCreate (semEmpty);
    if (!pBuf->readSignal) {
        return -1;
    }

    pBuf->writeSignal = semBinaryCreate (semEmpty);
    if (!pBuf->writeSignal) {
        semBinaryDestroy (pBuf->readSignal);
        return -1;
    }

    pBuf->readLock = semMutexCreate ();
    if (!pBuf->readLock) {
        semBinaryDestroy (pBuf->readSignal);
        semBinaryDestroy (pBuf->writeSignal);
        return -1;
    }

    pBuf->writeLock = semMutexCreate ();
    if (!pBuf->writeLock) {
        semBinaryDestroy (pBuf->readSignal);
        semBinaryDestroy (pBuf->writeSignal);
        semMutexDestroy (pBuf->readLock);
        return -1;
    }

    return 0;
}

/*
 * cacRingBufferDestroy ()
 */
void cacRingBufferDestroy (ringBuffer *pBuf)
{
    /*
     * force any read/write/reserve/commit ops in
     * other threads to complete
     */
    pBuf->shutDown = 1u;
    semBinaryGive (pBuf->readSignal);
    semBinaryGive (pBuf->writeSignal);
    semMutexMustTake (pBuf->readLock);
    semMutexMustTake (pBuf->writeLock);

    /*
     * clean up
     */
    semBinaryDestroy (pBuf->readSignal);
    semBinaryDestroy (pBuf->writeSignal);
    semMutexDestroy (pBuf->readLock);
    semMutexDestroy (pBuf->writeLock);
}

/*
 * cacRingBufferShutDown ();
 */
void cacRingBufferShutDown (ringBuffer *pBuf)
{
    pBuf->shutDown = 1u;
    semBinaryGive (pBuf->readSignal);
    semBinaryGive (pBuf->writeSignal);
}

/*
 * cacRingBufferReadSize ()
 */
static inline unsigned cacRingBufferReadSize (ringBuffer *pBuf)
{
    unsigned long   count;
    
    if ( pBuf->wtix <= pBuf->rdix ) {
        static const unsigned bufSizeM1 = sizeof (pBuf->buf) - 1u;
        count = ( bufSizeM1 - pBuf->rdix ) + pBuf->wtix;
    }
    else  {
        count = (pBuf->wtix - pBuf->rdix) - 1u;
    }
    return count;
}

/*
 * cacRingBufferContiguousReadSize ()
 */
static inline unsigned cacRingBufferContiguousReadSize (ringBuffer *pBuf)
{
    static const unsigned bufSizeM1 = sizeof (pBuf->buf) - 1u;
    unsigned long   count;
    
    if ( pBuf->wtix <= pBuf->rdix ) {
        if (pBuf->rdix==bufSizeM1) {
            count = pBuf->wtix;
        }
        else {
            count = bufSizeM1 - pBuf->rdix;
        }
    }
    else {
        count = (pBuf->wtix - pBuf->rdix) - 1u;
    }
    return count;
}

/*
 * cacRingBufferWriteSize ()
 */
static inline unsigned cacRingBufferWriteSize (ringBuffer *pBuf)
{
    unsigned long   count;

    if (pBuf->wtix <= pBuf->rdix) {
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
static inline unsigned cacRingBufferContiguousWriteSize (ringBuffer *pBuf)
{
    unsigned long   count;

    if (pBuf->wtix <= pBuf->rdix) {
        count = pBuf->rdix - pBuf->wtix;
    }
    else {
        count = sizeof (pBuf->buf) - pBuf->wtix;
    }
    return count;
}


/*
 * cacRingBufferReadPartial ()
 *
 * returns the number of bytes read which may be less than
 * the number requested.
 */
static unsigned cacRingBufferReadPartial (ringBuffer *pRing, void *pBuf,
                unsigned nBytes)
{
    unsigned totalBytes;

    if ( pRing->wtix < pRing->rdix ) {
        static const unsigned bufSizeM1 = sizeof (pRing->buf) - 1u;
        unsigned nBytesAvail1stBlock, nBytesAvail2ndBlock;

        nBytesAvail1stBlock = bufSizeM1 - pRing->rdix;
        nBytesAvail2ndBlock = pRing->wtix;
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
    else if ( pRing->wtix > pRing->rdix ) {
        totalBytes = (pRing->wtix - pRing->rdix) - 1;
        if ( totalBytes > nBytes ) {
            totalBytes = nBytes;
        }
        memcpy (pBuf, pRing->buf+pRing->rdix+1, totalBytes);
        pRing->rdix += totalBytes;
        pRing->rdix &= ringIndexMask;
    }
    else {
        totalBytes = 0;
    }

    return totalBytes;
}

/*
 * cacRingBufferRead ()
 *
 * returns the number of bytes read which may be less than
 * the number requested.
 */
unsigned cacRingBufferRead (ringBuffer *pRing, void *pBuf,
                unsigned nBytes)
{
    unsigned char *pBufTmp = (unsigned char *) pBuf;
    unsigned totalBytes = 0;
    unsigned curBytes;

    semMutexMustTake (pRing->readLock);

    while (totalBytes<nBytes) {
        curBytes = cacRingBufferReadPartial (pRing, 
                        pBufTmp+totalBytes, nBytes-totalBytes);
        if (curBytes==0) {
            semBinaryMustTake (pRing->readSignal);
            if (pRing->shutDown) {
                semMutexGive (pRing->readLock);
                return totalBytes;
            }
        }
        else {
            totalBytes += curBytes;
        }
    }

    semMutexGive (pRing->readLock);

    return totalBytes;
}

/*
 * cacRingBufferWritePartial ()
 *
 * returns the number of bytes written which may be less than
 * the number requested.
 */
static unsigned cacRingBufferWritePartial (ringBuffer *pRing, 
            const void *pBuf, unsigned nBytes)
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

/*
 * cacRingBufferWrite ()
 *
 * returns the number of bytes written which may be less than
 * the number requested.
 */
unsigned cacRingBufferWrite (ringBuffer *pRing, const void *pBuf, 
                                  unsigned nBytes)
{
    unsigned char *pBufTmp = (unsigned char *) pBuf;
    unsigned totalBytes = 0;
    unsigned curBytes;

    semMutexMustTake (pRing->writeLock);

    while (totalBytes<nBytes) {
        curBytes = cacRingBufferWritePartial (pRing, 
                        pBufTmp+totalBytes, nBytes-totalBytes);
        if (curBytes==0) {
            semBinaryGive (pRing->readSignal);
            semBinaryMustTake (pRing->writeSignal);
            if (pRing->shutDown) {
                semMutexGive (pRing->writeLock);
                return totalBytes;
            }
        }
        else {
            totalBytes += curBytes;
        }
    }

    semMutexGive (pRing->writeLock);

    return totalBytes;
}

bool cacRingBufferWriteLockNoBlock (ringBuffer *pBuf, unsigned bytesRequired)
{
    semMutexMustTake (pBuf->writeLock);

    if (cacRingBufferWriteSize (pBuf)<bytesRequired) {
        semMutexGive (pBuf->writeLock);
        return false;
    }
    return true;
}

void cacRingBufferWriteUnlock (ringBuffer *pBuf)
{
    semMutexGive (pBuf->writeLock);
}

void *cacRingBufferWriteReserve (ringBuffer *pRing, unsigned *pBytesAvail)
{
    unsigned avail;

    semMutexMustTake (pRing->writeLock);
    
    avail = cacRingBufferContiguousWriteSize (pRing);
    while (avail==0) {
        semBinaryGive (pRing->readSignal);
        semBinaryMustTake (pRing->writeSignal);
        if (pRing->shutDown) {
            semMutexGive (pRing->writeLock);
            *pBytesAvail = 0u;
            return 0;
        }
        avail = cacRingBufferContiguousWriteSize (pRing);
    }

    *pBytesAvail = avail;

    return (void *) &pRing->buf[pRing->wtix];
}

void *cacRingBufferWriteReserveNoBlock (ringBuffer *pRing, unsigned *pBytesAvail)
{
    unsigned avail;

    semMutexMustTake (pRing->writeLock);
    
    avail = cacRingBufferContiguousWriteSize (pRing);

    if ( avail==0 || pRing->shutDown ) {
        *pBytesAvail = 0u;
        semMutexGive (pRing->writeLock);
        return NULL;
    }

    *pBytesAvail = avail;

    return (void *) &pRing->buf[pRing->wtix];
}

void cacRingBufferWriteCommit (ringBuffer *pRing, unsigned delta)
{
    pRing->wtix += delta;
    pRing->wtix &= ringIndexMask;
    semMutexGive (pRing->writeLock);
}

void *cacRingBufferReadReserve (ringBuffer *pRing, unsigned *pBytesAvail)
{
    unsigned avail;

    semMutexMustTake (pRing->readLock);
    
    avail = cacRingBufferContiguousReadSize (pRing);
    while (avail==0) {
        semBinaryMustTake (pRing->readSignal);
        if (pRing->shutDown) {
            semMutexGive (pRing->readLock);
            *pBytesAvail = 0u;
            return NULL;
        }
        avail = cacRingBufferContiguousReadSize (pRing);
    }

    *pBytesAvail = avail;

    return (void *) &pRing->buf[(pRing->rdix+1) & ringIndexMask];
}

void *cacRingBufferReadReserveNoBlock (ringBuffer *pRing, unsigned *pBytesAvail)
{
    unsigned avail;

    semMutexMustTake (pRing->readLock);
    
    avail = cacRingBufferContiguousReadSize (pRing);

    if ( avail==0 || pRing->shutDown ) {
        *pBytesAvail = 0u;
        semMutexGive (pRing->readLock);
        return NULL;
    }

    *pBytesAvail = avail;

    return (void *) &pRing->buf[(pRing->rdix+1) & ringIndexMask];
}


void cacRingBufferReadCommit (ringBuffer *pRing, unsigned delta)
{
    pRing->rdix += delta;
    pRing->rdix &= ringIndexMask;
    semMutexGive (pRing->readLock);
}

void cacRingBufferWriteFlush (ringBuffer *pRing)
{
    semBinaryGive (pRing->readSignal);
}

void cacRingBufferReadFlush (ringBuffer *pRing)
{
    semBinaryGive (pRing->writeSignal);
}
