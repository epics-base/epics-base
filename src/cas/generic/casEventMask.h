/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 */


#ifndef casEventMaskH
#define casEventMaskH

#include "resourceLib.h"
 
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
    
    casEventMask (casEventRegistry &reg, const char *pName);
    
    casEventMask () 
    {
        this->clear();
    }
    
    void show (unsigned level) const;
    
    int eventsSelected()
    {
        return this->mask!=0u;
    }
    int noEventsSelected()
    {
        return this->mask==0u;
    }
    
    inline void operator|= (const casEventMask &rhs);
    inline void operator&= (const casEventMask &rhs);
    
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

