
//
// Example EPICS CA server
//

#include "exServer.h"

//
// exChannel::setOwner ()
//
void exChannel::setOwner(const char * const pUserName, 
		const char * const pHostName)
{
}

//
// exChannel::readAccess ()
//
aitBool exChannel::readAccess () const
{
	return aitTrue;
}

//
// exChannel::writeAccess ()
//
aitBool exChannel::writeAccess () const
{
	return aitTrue;
}


