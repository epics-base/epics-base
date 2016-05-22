/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casIntfIOh
#define casIntfIOh 

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casIntfIOh
#   undef epicsExportSharedSymbols
#endif

// external headers included here
#include "osiSock.h"

#ifdef epicsExportSharedSymbols_casIntfIOh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

class caNetAddr;
class caServerI;
class clientBufMemoryManager;

//
// casIntfIO
//
class casIntfIO {
public:
	casIntfIO ( const caNetAddr & addr );
	virtual ~casIntfIO ();
	void show ( unsigned level ) const;

	int getFD () const;

	void setNonBlocking ();

	// 
	// called when we expect that a virtual circuit for a
	// client can be created
	// 
	class casStreamOS * newStreamClient ( caServerI & cas, 
        clientBufMemoryManager & ) const;

    caNetAddr serverAddress () const;
    
private:
	SOCKET sock;
	struct sockaddr_in addr;
};

#endif // casIntfIOh
