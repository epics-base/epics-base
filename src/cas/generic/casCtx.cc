/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "server.h"

//
// casCtx::show()
//
void casCtx::show (unsigned level) const
{
	printf ("casCtx at %p\n", this);
	if (level >= 3u) {
		printf ("\tpMsg = %p\n", &this->msg);
		printf ("\tpData = %p\n", pData);
		printf ("\tpCAS = %p\n", pCAS);
		printf ("\tpClient = %p\n", pClient);
		printf ("\tpChannel = %p\n", pChannel);
		printf ("\tpPV = %p\n", pPV);
	}
}

