
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

#ifndef sgAutoPtrh
#define sgAutoPtrh

template < class T >
class sgAutoPtr {
public:
    sgAutoPtr ( struct CASG & );
    ~sgAutoPtr ();
    sgAutoPtr < T > & operator = ( T * );
    T * operator -> ();
    T & operator * ();
    T * get ();
    T * release ();
private:
    T * pNotify;
    struct CASG & sg;
};

template < class T >
inline sgAutoPtr < T > :: sgAutoPtr ( struct CASG & sgIn  ) : 
    pNotify ( 0 ), sg ( sgIn )
{
}

template < class T >
inline sgAutoPtr < T > :: ~sgAutoPtr () 
{
    this->sg.destroyPendingIO ( this->pNotify );
}

template < class T >
inline sgAutoPtr < T > & sgAutoPtr < T > :: operator = ( T * pNotifyIn )
{
    if ( this->pNotify ) {
        this->sg.destroyPendingIO ( this->pNotify );
    }
    this->pNotify = pNotifyIn;
    return *this;
}

template < class T >
inline T * sgAutoPtr < T > :: operator -> ()
{
    return this->pNotify;
}

template < class T >
inline T & sgAutoPtr < T > :: operator * ()
{
    assert ( this->pNotify );
    return * this->pNotify;
}

template < class T >
inline T * sgAutoPtr < T > :: release ()
{
    T * pTmp = this->pNotify;
    this->pNotify = 0;
    return pTmp;
}

template < class T >
inline T * sgAutoPtr < T > :: get ()
{
    return this->pNotify;
}

#endif // sgAutoPtrh
