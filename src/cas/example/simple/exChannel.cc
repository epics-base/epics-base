
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


