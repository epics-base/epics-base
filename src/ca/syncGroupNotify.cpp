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
 *  $Id$
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "syncGroup.h"
#include "oldAccess.h"

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
    ::printf ( "pending sg op: chan=%s magic=%u sg=%p\n",
         this->chan->pName(), this->magic, 
         static_cast <void *> ( &this->sg ) );
}


