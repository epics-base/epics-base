
/*  
 *  $Id$
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
    T * get () const;
    T * release ();
private:
    T *p;
};

template < class T >
inline autoPtrDestroy<T>::autoPtrDestroy ( T *pIn ) :
    p ( pIn ) {}

template < class T >
inline autoPtrDestroy<T>::~autoPtrDestroy ()
{
    if ( this->p ) {
        p->destroy ();
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
