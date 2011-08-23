/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdio.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "casCtx.h"
#include "caServerI.h"
#include "casCoreClient.h"
#include "casChannelI.h"

casCtx::casCtx() :
	pData ( NULL ), pCAS ( NULL ), pClient ( NULL ),
	pChannel ( NULL ), pPV ( NULL ), nAsyncIO ( 0u )
{
	memset(&this->msg, 0, sizeof(this->msg));
}

void casCtx::show ( unsigned level ) const
{
	printf ( "casCtx at %p\n", 
        static_cast <const void *> ( this ) );
	if (level >= 3u) {
		printf ("\tpMsg = %p\n", 
            static_cast <const void *> ( &this->msg ) );
		printf ("\tpData = %p\n", 
            static_cast <void *> ( pData ) );
		printf ("\tpCAS = %p\n", 
            static_cast <void *> ( pCAS ) );
		printf ("\tpClient = %p\n", 
            static_cast <void *> ( pClient ) );
		printf ("\tpChannel = %p\n", 
            static_cast <void *> ( pChannel ) );
		printf ("\tpPV = %p\n", 
            static_cast <void *> ( pPV ) );
	}
}

