/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*  
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef autoPtrDestroyh
#define autoPtrDestroyh

template < class T >
class autoPtrDestroy {
public:
    autoPtrDestroy ( T * );
    ~autoPtrDestroy ();
    T & operator * () const;
    T * operator -> () const;
    autoPtrDestroy<T> & operator = ( T * );
    T * get () const;
    T * release ();
private:
    T * p;
    // not implemented
    autoPtrDestroy<T> & operator = ( const autoPtrDestroy<T> & );
    autoPtrDestroy ( const autoPtrDestroy<T> & );
};

template < class T >
inline autoPtrDestroy<T>::autoPtrDestroy ( T *pIn ) :
    p ( pIn ) {}

template < class T >
inline autoPtrDestroy<T>::~autoPtrDestroy ()
{
    if ( this->p ) {
        this->p->destroy ();
    }
}

template < class T >
inline T & autoPtrDestroy<T>::operator * () const
{
    return * this->p;
}

template < class T >
inline T * autoPtrDestroy<T>::operator -> () const
{
    return this->p;
}

template < class T >
inline autoPtrDestroy<T> & autoPtrDestroy<T>::operator = ( T * pIn )
{
    if ( this->p ) {
        this->p->destroy ();
    }
    this->p = pIn;
    return *this;
}

template < class T >
inline T * autoPtrDestroy<T>::get () const
{
    return this->p;
}

template < class T >
inline T * autoPtrDestroy<T>::release ()
{
    T *pTmp = this->p;
    this->p = 0;
    return pTmp;
}

#endif // #ifdef autoPtrDestroyh
