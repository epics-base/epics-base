

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

#ifndef ringBufferh
#define ringBufferh

#include "osiSem.h"

#define nBitsRingIndex 14
#define nElementsInRing (1<<nBitsRingIndex)

typedef struct ringBuffer {
    char                buf[nElementsInRing];
    semBinaryId         readSignal;
    semBinaryId         writeSignal;
    semMutexId          readLock;
    semMutexId          writeLock;
    unsigned            rdix; /* index of last char read  */ 
    unsigned            wtix; /* index of next char to write */
    unsigned            shutDown;
} ringBuffer;

int cacRingBufferConstruct (ringBuffer *pBuf);
void cacRingBufferDestroy (ringBuffer *pBuf);

unsigned cacRingBufferWrite (ringBuffer *pRing, 
          const void *pBuf, unsigned nBytes);

unsigned cacRingBufferRead (ringBuffer *pRing, 
                                 void *pBuf, unsigned nBytes);

bool cacRingBufferWriteLockNoBlock (ringBuffer *pBuf, unsigned bytesRequired);

void cacRingBufferWriteUnlock (ringBuffer *pBuf);

void *cacRingBufferWriteReserve (ringBuffer *pBuf, unsigned *pAvailBytes);

void cacRingBufferWriteCommit (ringBuffer *pBuf, unsigned delta);

void *cacRingBufferReadReserve (ringBuffer *pBuf, unsigned *pBytesAvail);

void *cacRingBufferReadReserveNoBlock (ringBuffer *pBuf, unsigned *pBytesAvail);

void cacRingBufferReadCommit (ringBuffer *pBuf, unsigned delta);

void cacRingBufferReadFlush (ringBuffer *pBuf);
void cacRingBufferWriteFlush (ringBuffer *pBuf);

void cacRingBufferShutDown (ringBuffer *pBuf);

#endif /* ringBufferh */
