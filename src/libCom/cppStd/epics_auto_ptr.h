//  epics_auto_ptr.h
//	Author:	Jeff Hill
//	Date:	March 2001

#ifndef epics_auto_ptrh
#define epics_auto_ptrh

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
	T* get () const;                 
	T* release ();                   
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
inline T * epics_auto_ptr<T>::get() const
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

#endif // ifndef epics_auto_ptrh
