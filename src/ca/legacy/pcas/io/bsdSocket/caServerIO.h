/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef caServerIOh
#define caServerIOh

#include "casdef.h"

//
// caServerIO
//
class caServerIO {
public:
	caServerIO ();
	virtual ~caServerIO();

	//
	// show status of IO subsystem
	//
	void show ( unsigned level ) const;

    void locateInterfaces ();

private:

	//
	// static member data
	//
	static int staticInitialized;
	//
	// static member func
	//
	static inline void staticInit ();

	virtual caStatus attachInterface (
        const caNetAddr & addr, bool autoBeaconAddr,
		bool addConfigAddr ) = 0;

    virtual void addMCast(const osiSockAddr&) = 0;
};

#endif // caServerIOh
