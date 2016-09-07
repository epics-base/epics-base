
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include "errlog.h"

#define epicsExportSharedSymbols
#include "chanIntfForPV.h"
#include "casCoreClient.h"
#include "casPVI.h"

chanIntfForPV::chanIntfForPV ( class casCoreClient & clientIn, 
                              casChannelDestroyFromPV & destroyRefIn ) :
    clientRef ( clientIn ), destroyRef ( destroyRefIn )
{
}

chanIntfForPV::~chanIntfForPV () 
{
    while ( casMonitor * pMon = this->monitorList.get () ) {
	    this->clientRef.destroyMonitor ( *pMon );
    }
}

class casCoreClient & chanIntfForPV::client () const
{
    return this->clientRef;
}

void chanIntfForPV::installMonitor ( casPVI & pv, casMonitor & mon )
{
	caStatus status = pv.installMonitor ( 
                    mon, this->monitorList );
	if ( status ) {
		errMessage ( status,
			"Server tool failed to register event\n" );
	}
}

void chanIntfForPV::show ( unsigned level ) const
{
	printf ( "chanIntfForPV\n" );
    if ( level > 0 && this->monitorList.count() ) {
		printf ( "List of subscriptions attached\n" );
	    tsDLIterConst < casMonitor > iter = 
            this->monitorList.firstIter ();
	    while ( iter.valid () ) {
		    iter->show ( level - 1 );
		    ++iter;
	    }
    }
}


