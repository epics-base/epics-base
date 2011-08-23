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
// Example EPICS CA server
//

#include "exServer.h"

//
// exChannel::setOwner ()
//
void exChannel::setOwner(const char * const /* pUserName */, 
        const char * const /* pHostName */)
{
}

//
// exChannel::readAccess ()
//
bool exChannel::readAccess () const
{
    return true;
}

//
// exChannel::writeAccess ()
//
bool exChannel::writeAccess () const
{
    return true;
}


