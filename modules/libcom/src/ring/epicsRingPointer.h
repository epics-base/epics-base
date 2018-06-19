/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2012 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author:  Marty Kraimer Date:    15JUL99
 *          Ralph Lange <Ralph.Lange@gmx.de>
 */

#ifndef INCepicsRingPointerh
#define INCepicsRingPointerh

/* NOTES
 *   If there is only one writer it is not necessary to lock push
 *   If there is a single reader it is not necessary to lock pop
 *
 *   epicsRingPointerLocked uses a spinlock.
 */

#include "epicsSpin.h"
#include "shareLib.h"

#ifdef __cplusplus
template <class T>
class epicsRingPointer {
public: /* Functions */
    epicsRingPointer(int size, bool locked);
    ~epicsRingPointer();
    bool push(T *p);
    T* pop();
    void flush();
    int getFree() const;
    int getUsed() const;
    int getSize() const;
    bool isEmpty() const;
    bool isFull() const;

private: /* Prevent compiler-generated member functions */
    /* default constructor, copy constructor, assignment operator */
    epicsRingPointer();
    epicsRingPointer(const epicsRingPointer &);
    epicsRingPointer& operator=(const epicsRingPointer &);

private: /* Data */
    epicsSpinId lock;
    volatile int nextPush;
    volatile int nextPop;
    int size;
    T  * volatile * buffer;
};

extern "C" {
#endif /*__cplusplus */

typedef void *epicsRingPointerId;

epicsShareFunc epicsRingPointerId  epicsShareAPI epicsRingPointerCreate(int size);
/* Same, but secured by a spinlock */
epicsShareFunc epicsRingPointerId  epicsShareAPI epicsRingPointerLockedCreate(int size);
epicsShareFunc void epicsShareAPI epicsRingPointerDelete(epicsRingPointerId id);
/*ringPointerPush returns (0,1) if p (was not, was) put on ring*/
epicsShareFunc int  epicsShareAPI epicsRingPointerPush(epicsRingPointerId id,void *p);
/*ringPointerPop returns 0 if ring is empty*/
epicsShareFunc void* epicsShareAPI epicsRingPointerPop(epicsRingPointerId id) ;
epicsShareFunc void epicsShareAPI epicsRingPointerFlush(epicsRingPointerId id);
epicsShareFunc int  epicsShareAPI epicsRingPointerGetFree(epicsRingPointerId id);
epicsShareFunc int  epicsShareAPI epicsRingPointerGetUsed(epicsRingPointerId id);
epicsShareFunc int  epicsShareAPI epicsRingPointerGetSize(epicsRingPointerId id);
epicsShareFunc int  epicsShareAPI epicsRingPointerIsEmpty(epicsRingPointerId id);
epicsShareFunc int  epicsShareAPI epicsRingPointerIsFull(epicsRingPointerId id);

/* This routine was incorrectly named in previous releases */
#define epicsRingPointerSize epicsRingPointerGetSize

#ifdef __cplusplus
}
#endif
/* END OF DECLARATIONS */

/* INLINE FUNCTIONS */

/* Algorithm note
 *  Space is allocated for one additional element.
 *  A put request is rejected if the it would cause nextPush to equal nextPop
 *  The algorithm does not require locking puts for a single writer
 *      or locking of gets for a single reader
 */
#ifdef __cplusplus

template <class T>
inline epicsRingPointer<T>::epicsRingPointer(int sz, bool locked) :
    lock(0), nextPush(0), nextPop(0), size(sz+1), buffer(new T* [sz+1])
{
    if (locked)
        lock = epicsSpinCreate();
}

template <class T>
inline epicsRingPointer<T>::~epicsRingPointer()
{
    if (lock) epicsSpinDestroy(lock);
    delete [] buffer;
}

template <class T>
inline bool epicsRingPointer<T>::push(T *p)
{
    if (lock) epicsSpinLock(lock);
    int next = nextPush;
    int newNext = next + 1;
    if(newNext>=size) newNext=0;
    if (newNext == nextPop) {
        if (lock) epicsSpinUnlock(lock);
        return(false);
    }
    buffer[next] = p;
    nextPush = newNext;
    if (lock) epicsSpinUnlock(lock);
    return(true);
}

template <class T>
inline T* epicsRingPointer<T>::pop()
{
    if (lock) epicsSpinLock(lock);
    int next = nextPop;
    if (next == nextPush) {
        if (lock) epicsSpinUnlock(lock);
        return(0);
    }
    T*p  = buffer[next];
    ++next;
    if(next >=size) next = 0;
    nextPop = next;
    if (lock) epicsSpinUnlock(lock);
    return(p);
}

template <class T>
inline void epicsRingPointer<T>::flush()
{
    if (lock) epicsSpinLock(lock);
    nextPop = 0;
    nextPush = 0;
    if (lock) epicsSpinUnlock(lock);
}

template <class T>
inline int epicsRingPointer<T>::getFree() const
{
    if (lock) epicsSpinLock(lock);
    int n = nextPop - nextPush - 1;
    if (n < 0) n += size;
    if (lock) epicsSpinUnlock(lock);
    return n;
}

template <class T>
inline int epicsRingPointer<T>::getUsed() const
{
    if (lock) epicsSpinLock(lock);
    int n = nextPush - nextPop;
    if (n < 0) n += size;
    if (lock) epicsSpinUnlock(lock);
    return n;
}

template <class T>
inline int epicsRingPointer<T>::getSize() const
{
    return(size-1);
}

template <class T>
inline bool epicsRingPointer<T>::isEmpty() const
{
    bool isEmpty;
    if (lock) epicsSpinLock(lock);
    isEmpty = (nextPush == nextPop);
    if (lock) epicsSpinUnlock(lock);
    return isEmpty;
}

template <class T>
inline bool epicsRingPointer<T>::isFull() const
{
    if (lock) epicsSpinLock(lock);
    int count = nextPush - nextPop +1;
    if (lock) epicsSpinUnlock(lock);
    return((count == 0) || (count == size));
}

#endif /* __cplusplus */

#endif /* INCepicsRingPointerh */
