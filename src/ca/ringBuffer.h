

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
    semMutexId          readLock;
    semMutexId          writeLock;
    unsigned            rdix; /* index of last char read  */ 
    unsigned            wtix; /* index of next char to write */
    char                buf[nElementsInRing];
} ringBuffer;

bool cacRingBufferConstruct ( ringBuffer *pBuf );

void cacRingBufferDestroy ( ringBuffer *pBuf );

void cacRingBufferWriteLock ( ringBuffer *pBuf );

bool cacRingBufferWriteLockIfBytesAvailable ( ringBuffer *pBuf, unsigned bytesRequired );

void cacRingBufferWriteUnlock ( ringBuffer *pBuf );

unsigned cacRingBufferWrite ( ringBuffer *pBuf, const void *pMsg, unsigned nBytes );

bool cacRingBufferWriteNoBlock ( ringBuffer *pBuf, const void *pMsg, unsigned nBytes );

struct msgDescriptor {
    const void *pMsg;
    unsigned length;
};

bool cacRingBufferWriteMultipartMessageNoBlock ( ringBuffer *pBuf, 
                     const msgDescriptor *pMsgs, unsigned nMsgs );

void *cacRingBufferWriteReserve ( ringBuffer *pBuf, unsigned *pBytesAvail );

void cacRingBufferWriteCommit ( ringBuffer *pBuf, unsigned delta );

void *cacRingBufferReadReserve ( ringBuffer *pBuf, unsigned *pBytesAvail );

bool cacRingBufferReadNoBlock ( ringBuffer *pBuf, void *pDest, unsigned nBytesRequired );

void cacRingBufferReadCommit ( ringBuffer *pBuf, unsigned delta );

#endif /* ringBufferh */
