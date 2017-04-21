/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
// casOSD.h - Channel Access Server OS dependent wrapper
// 

#ifndef casIntfOSh
#define casIntfOSh 

#include "casIntfIO.h"
#include "casDGIntfOS.h"

//
// casIntfOS
//
class casIntfOS : public casIntfIO, public tsDLNode < casIntfOS >, 
    public casDGIntfOS
{
	friend class casServerReg;
public:
	casIntfOS ( caServerI &, clientBufMemoryManager &, const caNetAddr &, 
        bool autoBeaconAddr = true, bool addConfigBeaconAddr = false );
	virtual ~casIntfOS();
    void show ( unsigned level ) const;
    caNetAddr serverAddress () const;
private:
	caServerI & cas;
	class casServerReg * pRdReg;

	casIntfOS ( const casIntfOS & );
	casIntfOS & operator = ( const casIntfOS & );
};

#endif // casIntfOSh
