
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

#ifndef autoPtrRecycleh
#define autoPtrRecycleh

template < class T >
class autoPtrRecycle {
public:
    autoPtrRecycle ( cacRecycle &, T * );
    ~autoPtrRecycle ();
    T & operator * () const;
    T * operator -> () const;
    T * get () const;
    T * release ();
private:
    T *p;
    cacRecycle &r;
};

template < class T >
inline autoPtrRecycle<T>::autoPtrRecycle ( cacRecycle &rIn, T *pIn ) :
    p ( pIn ), r ( rIn ) {}

template < class T >
inline autoPtrRecycle<T>::~autoPtrRecycle ()
{
    if ( this->p ) {
        baseNMIU *pb = this->p;
        pb->destroy ( this->r );
    }
}

template < class T >
inline T & autoPtrRecycle<T>::operator * () const
{
    return * this->p;
}

template < class T >
inline T * autoPtrRecycle<T>::operator -> () const
{
    return this->p;
}

template < class T >
inline T * autoPtrRecycle<T>::get () const
{
    return this->p;
}

template < class T >
inline T * autoPtrRecycle<T>::release ()
{
    T *pTmp = this->p;
    this->p = 0;
    return pTmp;
}

#endif // #ifdef autoPtrRecycleh
