
#ifndef netiiuPtr_ILh
#define netiiuPtr_ILh

inline netiiuPtr::netiiuPtr () : piiu ( 0 ) 
{
}

inline netiiuPtr::~netiiuPtr () 
{
}

inline netiiu *netiiuPtr::pIIU () const 
{ 
    return this->piiu;
}

inline void netiiuPtr::setPointer ( netiiu *p )
{
    this->piiu = p;
}


#endif // netiiuPtr_ILh