
/*
 *  $Id$
 *
 *  Author: Jeff O. Hill
 *
 */

#ifndef epicsSingleton_h
#define epicsSingleton_h

#include "shareLib.h"

class epicsShareClass epicsSingletonBase {
public:
    epicsSingletonBase ();
protected:
    virtual ~epicsSingletonBase ();
    void lockedFactory ();
    void * singletonPointer () const;
private:
    void * pSingleton;
    static class epicsMutex mutex;
    virtual void * factory () = 0;
};

template <class T>
class epicsSingleton : private epicsSingletonBase {
public:
    virtual ~epicsSingleton ();
    T * operator -> ();
    T & operator * ();
private:
    void * factory ();
};

inline epicsSingletonBase::epicsSingletonBase () : pSingleton ( 0 )
{
}

inline void * epicsSingletonBase::singletonPointer () const
{
    return this->pSingleton;
}

template < class T >
inline epicsSingleton<T>::~epicsSingleton ()
{
    delete static_cast < T * > ( this->singletonPointer () );
}

template < class T >
inline T * epicsSingleton<T>::operator -> ()
{
    if ( ! this->singletonPointer () ) {
        this->lockedFactory ();
    }
    return static_cast < T * > ( this->singletonPointer () );
}

template < class T >
inline T & epicsSingleton<T>::operator * ()
{
    if ( ! this->singletonPointer () ) {
        this->lockedFactory ();
    }
    return * static_cast < T * > ( this->singletonPointer () );
}

template < class T >
void * epicsSingleton<T>::factory ()
{
    return static_cast < void * > ( new T );
}

#endif // epicsSingleton_h

