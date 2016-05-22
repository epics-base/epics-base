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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */


#ifndef casEventMaskH
#define casEventMaskH

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casEventMaskH
#   undef epicsExportSharedSymbols
#endif
#include "resourceLib.h"
#ifdef epicsExportSharedSymbols_casEventMaskH
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif
 
class casEventRegistry;

class epicsShareClass casEventMask {
    friend inline casEventMask operator| (const casEventMask &lhs, 
        const casEventMask &rhs);
    friend inline casEventMask operator& (const casEventMask &lhs, 
        const casEventMask &rhs);
    friend inline int operator== (const casEventMask &lhs, 
        const casEventMask &rhs);
    friend inline int operator!= (const casEventMask &lhs, 
        const casEventMask &rhs);
    friend class casEventRegistry;
public:
    void clear ()
    {
        this->mask = 0u;
    }
    
    casEventMask ( casEventRegistry &reg, const char *pName );
    
    casEventMask () 
    {
        this->clear();
    }
    
    void show ( unsigned level ) const;
    
    bool eventsSelected () const
    {
        return this->mask != 0u;
    }
    bool noEventsSelected () const
    {
        return this->mask == 0u;
    }
    
    inline void operator |= ( const casEventMask & rhs );
    inline void operator &= ( const casEventMask & rhs );
    
private:
    unsigned mask;
};

inline casEventMask operator| (const casEventMask &lhs, const casEventMask &rhs)
{
        casEventMask result;
 
        result.mask = lhs.mask | rhs.mask;
        return result;
}
inline casEventMask operator& (const casEventMask &lhs, const casEventMask &rhs)
{
        casEventMask result;
 
        result.mask = lhs.mask & rhs.mask;
        return result;
}

inline int operator== (const casEventMask &lhs, const casEventMask &rhs)
{
        return lhs.mask == rhs.mask;
}

inline int operator!= (const casEventMask &lhs, const casEventMask &rhs)
{
        return lhs.mask != rhs.mask;
}

inline void casEventMask::operator|= (const casEventMask &rhs) {
	*this = *this | rhs;		
}

inline void casEventMask::operator&= (const casEventMask &rhs) {
	*this = *this & rhs;		
}

#endif // casEventMaskH

