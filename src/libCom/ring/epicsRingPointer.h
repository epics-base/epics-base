/*epicsRingPointer.h */

/* Author:  Marty Kraimer Date:    15JUL99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#ifndef INCepicsRingPointerh
#define INCepicsRingPointerh

/* NOTES
    If there is only one writer it is not necessary to lock push
    If there is a single reader it is not necessary to lock pop
*/

#include "shareLib.h"

#ifdef __cplusplus
template <class T> class epicsRingPointer
{
public:
    epicsRingPointer(int size);
    ~epicsRingPointer();
    bool push(T *p);
    T* pop();
    void flush();
    int getFree() const;
    int getUsed() const;
    int getSize() const;
    bool isEmpty() const;
    bool isFull() const;
private:
    int nextPush;
    int nextPop;
    int size;
    T **buffer;
    // copy constructor and assignment operator not allowed
    epicsRingPointer(const epicsRingPointer &);
    epicsRingPointer& operator=(const epicsRingPointer &);
};
#endif /*__cplusplus */

#ifdef __cplusplus
extern "C" {
#endif

typedef void *epicsRingPointerId;

epicsShareFunc epicsRingPointerId  epicsShareAPI epicsRingPointerCreate(int size);
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

#ifdef __cplusplus
}
#endif

/* Following is implementation */
/* Algorithm note
*  Space is allocated for one additional element.
*  A put request is rejected if the it would cause nextPush to equal nextPop
*  The algorithm does not require locking puts for a single writer
*      or locking of gets for a single reader
*/
#ifdef __cplusplus

template <class T> inline epicsRingPointer<T>::epicsRingPointer(int sz)
: nextPush(0), nextPop(0), size(sz+1), buffer(new T* [sz+1]) {}

template <class T> inline epicsRingPointer<T>::~epicsRingPointer()
{delete [] buffer;}

template <class T> inline bool epicsRingPointer<T>::push(T *p)
{
    int newNext = nextPush +1;
    if(newNext>=size) newNext=0;
    if(newNext==nextPop) return(false);
    buffer[nextPush] = p;
    nextPush = newNext;
    return(true);
}

template <class T> inline T* epicsRingPointer<T>::pop()
{
    if(nextPop == nextPush) return(0);
    T*p  = buffer[nextPop];
    int newNext= nextPop;
    ++newNext;
    if(newNext >=size) newNext = 0;
    nextPop = newNext;
    return(p);
}

template <class T> inline void epicsRingPointer<T>::flush()
{ nextPop = nextPush = 0;}

template <class T> inline int epicsRingPointer<T>::getFree() const
{
    int n = nextPop - nextPush - 1;
    if (n < 0) n += size;
    return n;
}

template <class T> inline int epicsRingPointer<T>::getUsed() const
{
    int n = nextPush - nextPop;
    if (n < 0) n += size;
    return n;
}

template <class T> inline int epicsRingPointer<T>::getSize() const
{return(size-1);}

template <class T> inline bool epicsRingPointer<T>::isEmpty() const
{return(nextPush==nextPop);}

template <class T> inline bool epicsRingPointer<T>::isFull() const
{
    int count = nextPush - nextPop +1;
    return((count == 0) || (count == size));
}

#endif /* __cplusplus */

#endif /* INCepicsRingPointerh */
