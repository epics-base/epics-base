
/*
 *  $Id$
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
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

#include "iocinf.h"
#include "oldAccess.h"
#include "oldChannelNotify_IL.h"

syncGroupNotify::syncGroupNotify ( CASG &sgIn, chid chanIn ) :
    chan ( chanIn ), sg ( sgIn ), 
        magic ( CASG_MAGIC ), id ( 0u ), idIsValid ( false )
{
}

syncGroupNotify::~syncGroupNotify ()
{
    if ( this->idIsValid ) {
        this->chan->ioCancel ( this->id );
    }
}

void syncGroupNotify::show ( unsigned /* level */ ) const
{
    printf ( "pending sg op: chan=%s magic=%u sg=%p\n",
         this->chan->pName(), this->magic, 
         static_cast <void *> ( &this->sg ) );
}


