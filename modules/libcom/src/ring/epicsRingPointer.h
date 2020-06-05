/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2012 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/**
 * \file epicsRingPointer.h
 * \brief A circular buffer to store pointers
 * \author Marty Kraimer, Ralph Lange
 *
 * \details
 * epicsRingPointer.h provides both C and C++ APIs for creating and using ring
 * buffers (first in first out circular buffers) that store pointers. The
 * unlocked kind is designed so that one writer thread and one reader thread
 * can access the ring simultaneously without requiring mutual exclusion. The
 * locked variant uses an epicsSpinLock, and works with any numbers of writer
 * and reader threads.
 * \note If there is only one writer it is not necessary to lock pushes.
 * If there is a single reader it is not necessary to lock pops.
 * epicsRingPointerLocked uses a spinlock.
 */

#ifndef INCepicsRingPointerh
#define INCepicsRingPointerh


#include "epicsSpin.h"
#include "libComAPI.h"

#ifdef __cplusplus
/**
 * \brief A C++ template class providing methods for creating and using a ring
 *  buffer (a first in, first out circular buffer) that stores pointers to
 *  objects of the template type.
 */
template <class T>
class epicsRingPointer {
public: /* Functions */
    /**\brief Constructor
     * \param size Maximum number of elements (pointers) that can be stored
     * \param locked If true, the spin lock secured variant is created
     */
    epicsRingPointer(int size, bool locked);
    /**\brief Destructor
     */
    ~epicsRingPointer();
    /**\brief Push a new entry on the ring
     * \return True on success, False if the buffer was full
     */
    bool push(T *p);
    /**\brief Take an element off the ring
     * \return The element, or NULL if the ring was empty
     */
    T* pop();
    /**\brief Remove all elements from the ring.
     * \note If this operation is performed on a ring buffer of the
     * unsecured kind, all access to the ring should be locked.
     */
    void flush();
    /**\brief Get how much free space remains in the ring
     * \return The number of additional elements the ring could hold.
     */
    int getFree() const;
    /**\brief Get how many elements are stored on the ring
     * \return The number of elements currently stored.
     */
    int getUsed() const;
    /**\brief Get the size of the ring
     * \return The \c size specified when the ring was created.
     */
    int getSize() const;
    /**\brief Test if the ring is currently empty
     * \return True if the ring is empty, otherwise false.
     */
    bool isEmpty() const;
    /**\brief Test if the ring is currently full
     * \return True if the ring is full, otherwise false.
     */
    bool isFull() const;
    /**\brief See how full the ring has got since it was last checked.
     *
     * Returns the maximum number of elements the ring
     * buffer has held since the water mark was last reset.
     * A new ring buffer starts with a water mark of 0.
     * \return Actual highwater mark
     */
    int getHighWaterMark() const;
    /**\brief Reset high water mark
     *
     * High water mark will be set to the current usage
     */
    void resetHighWaterMark();

private: /* Prevent compiler-generated member functions */
    /* default constructor, copy constructor, assignment operator */
    epicsRingPointer();
    epicsRingPointer(const epicsRingPointer &);
    epicsRingPointer& operator=(const epicsRingPointer &);
    int getUsedNoLock() const;

private: /* Data */
    epicsSpinId lock;
    volatile int nextPush;
    volatile int nextPop;
    int size;
    int highWaterMark;
    T  * volatile * buffer;
};

extern "C" {
#endif /*__cplusplus */

/** \brief An identifier for the C API to a ring buffer storing pointers */
typedef void *epicsRingPointerId;
typedef void const *epicsRingPointerIdConst;
/**
 * \brief Create a new ring buffer
 * \param size Size of ring buffer to create
 * \return Ring buffer identifier or NULL on failure
 */
LIBCOM_API epicsRingPointerId  epicsStdCall epicsRingPointerCreate(int size);
/**
 * \brief Create a new ring buffer, secured by a spinlock
 * \param size Size of ring buffer to create
 * \return Ring buffer identifier or NULL on failure
 */
LIBCOM_API epicsRingPointerId  epicsStdCall epicsRingPointerLockedCreate(int size);
/**
 * \brief Delete the ring buffer and free any associated memory
 * \param id Ring buffer identifier
 */
LIBCOM_API void epicsStdCall epicsRingPointerDelete(epicsRingPointerId id);
/**
 * \brief Push pointer into the ring buffer
 * \param id Ring buffer identifier
 * \param p Pointer to be pushed to the ring
 * \return 1 if the pointer was successfully pushed, 0 if the buffer was full
 */
LIBCOM_API int  epicsStdCall epicsRingPointerPush(epicsRingPointerId id,void *p);
/**
 * \brief Take an element off the ring
 * \param id Ring buffer identifier
 * \return The pointer from the buffer, or NULL if the ring was empty
 */
LIBCOM_API void* epicsStdCall epicsRingPointerPop(epicsRingPointerId id) ;
/**
 * \brief Remove all elements from the ring
 * \param id Ring buffer identifier
 * \note If this operation is performed on a ring buffer of the unsecured
 * kind, all access to the ring should be locked.
 */
LIBCOM_API void epicsStdCall epicsRingPointerFlush(epicsRingPointerId id);
/**
 * \brief Return the amount of empty space in the ring buffer
 * \param id Ring buffer identifier
 * \return The number of additional elements it could hold.
 */
LIBCOM_API int  epicsStdCall epicsRingPointerGetFree(epicsRingPointerId id);
/**
 * \brief Return the number of elements stored in the ring buffer
 * \param id Ring buffer identifier
 * \return The number of elements stored in the ring buffer
 */
LIBCOM_API int  epicsStdCall epicsRingPointerGetUsed(epicsRingPointerId id);
/**
 * \brief Return the size of the ring
 * \param id Ring buffer identifier
 * \return The size of the ring buffer, i.e. the value of \c size
 * given when the ring was created.
 */
LIBCOM_API int  epicsStdCall epicsRingPointerGetSize(epicsRingPointerId id);
/**
 * \brief Check if the ring buffer is currently empty
 * \param id Ring buffer identifier
 * \return 1 if the ring is empty, otherwise 0
 */
LIBCOM_API int  epicsStdCall epicsRingPointerIsEmpty(epicsRingPointerId id);
/**
 * \brief Check if the ring buffer is currently full
 * \param id Ring buffer identifier
 * \return 1 if the ring buffer is full, otherwise 0
 */
LIBCOM_API int  epicsStdCall epicsRingPointerIsFull(epicsRingPointerId id);
/**
 * \brief Get the Highwater mark of the ring buffer
 *
 * Returns the largest number of elements the ring buffer has held since
 * the water mark was last reset. A new ring buffer starts with a
 * water mark of 0.
 * \param id Ring buffer identifier
 * \return Actual Highwater mark
 */
LIBCOM_API int  epicsStdCall epicsRingPointerGetHighWaterMark(epicsRingPointerIdConst id);
/**
 * \brief Reset the Highwater mark of the ring buffer
 *
 * The Highwater mark will be set to the current usage
 * \param id Ring buffer identifier
 */
LIBCOM_API void epicsStdCall epicsRingPointerResetHighWaterMark(epicsRingPointerId id);

/* This routine was incorrectly named in previous releases */
#define epicsRingPointerSize epicsRingPointerGetSize

#ifdef __cplusplus
}
#endif
/* END OF DECLARATIONS */

/* INLINE FUNCTIONS */

#ifdef __cplusplus

template <class T>
inline epicsRingPointer<T>::epicsRingPointer(int sz, bool locked) :
    lock(0), nextPush(0), nextPop(0), size(sz+1), highWaterMark(0),
    buffer(new T* [sz+1])
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
    int used = getUsedNoLock();
    if (used > highWaterMark) highWaterMark = used;
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
inline int epicsRingPointer<T>::getUsedNoLock() const
{
    int n = nextPush - nextPop;
    if (n < 0) n += size;
    return n;
}

template <class T>
inline int epicsRingPointer<T>::getUsed() const
{
    if (lock) epicsSpinLock(lock);
    int n = getUsedNoLock();
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

template <class T>
inline int epicsRingPointer<T>::getHighWaterMark() const
{
    return highWaterMark;
}

template <class T>
inline void epicsRingPointer<T>::resetHighWaterMark()
{
    if (lock) epicsSpinLock(lock);
    highWaterMark = getUsedNoLock();
    if (lock) epicsSpinUnlock(lock);
}

#endif /* __cplusplus */

#endif /* INCepicsRingPointerh */
