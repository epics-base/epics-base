/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//  epicsMemoryH.h
//	Author:	Jeff Hill
//	Date:	March 2001

#ifndef epicsMemoryH
#define epicsMemoryH

template < class T >
class epics_auto_ptr {
public:	
    typedef T element_type;
    explicit epics_auto_ptr ( T *p = 0 );
    epics_auto_ptr ( const epics_auto_ptr<T> & rhs );
	~epics_auto_ptr();
    epics_auto_ptr<T> & operator = ( const epics_auto_ptr<T> & rhs );					
	T & operator * () const;
    T * operator -> () const;
	T * get () const;                 
	T * release (); 
    void reset ( T * p = 0 );
private:
    T *p;
};

template < class T >
inline epics_auto_ptr<T>::epics_auto_ptr ( T *pIn ) : 
p ( pIn ) {}

template < class T >
inline epics_auto_ptr<T>::epics_auto_ptr ( const epics_auto_ptr<T> & ap ) : 
p ( ap.release() ) {}

template < class T >
inline epics_auto_ptr<T>::~epics_auto_ptr () 
{
    delete this->p;
}

template < class T >	
inline epics_auto_ptr<T> & epics_auto_ptr<T>::operator = ( const epics_auto_ptr<T> & rhs )
{
    if ( &rhs != this) {
        delete this->p;
        this->p = rhs.release();
    }
    return *this;
}

template < class T >	
inline T & epics_auto_ptr<T>::operator * () const
{ 
    return * this->p; 
}

template < class T >	
inline T * epics_auto_ptr<T>::operator -> () const
{ 
    return this->p; 
}

template < class T >	
inline T * epics_auto_ptr<T>::get () const
{ 
    return this->p; 
}

template < class T >	
inline T * epics_auto_ptr<T>::release ()
{ 
    T *pTmp = this->p;
    this->p = 0;
    return pTmp; 
}

template < class T >	
inline void epics_auto_ptr<T>::reset ( T * pIn )
{
    delete this->p;
    this->p = pIn;
}

#endif // ifndef epicsMemoryH
