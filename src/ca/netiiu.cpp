
/*  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include "iocinf.h"

/*
 *  constructNIIU ()
 */
netiiu::netiiu (cac *pcac) : baseIIU (pcac)
{
    ellInit (&this->chidList);
}

/*
 *  netiiu::~netiiu ()
 */
netiiu::~netiiu ()
{
}

void netiiu::show ( unsigned /* level */ ) const
{
    this->pcas->lock ();

    tsDLIterConstBD <nciu> pChan ( this->chidList.first () );
	while ( pChan.valid () ) {
        char hostName [256];
		printf(	"%s native type=%d ", 
			pChan->pName (), pChan->nativeType () );
        pChan->hostName ( hostName, sizeof (hostName) );
		printf(	"N elements=%lu server=%s state=", 
			pChan->nativeElementCount (), hostName );
		switch ( pChan->state () ) {
		case cs_never_conn:
			printf ("never connected to an IOC");
			break;
		case cs_prev_conn:
			printf ("disconnected from IOC");
			break;
		case cs_conn:
			printf ("connected to an IOC");
			break;
		case cs_closed:
			printf ("invalid channel");
			break;
		default:
			break;
		}
		printf("\n");
	}

    this->pcas->unlock ();

}


