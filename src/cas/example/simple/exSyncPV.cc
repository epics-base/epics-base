
//
// Example EPICS CA server
//

#include <exServer.h>

//
// exSyncPV::exSyncPV()
//
exSyncPV::exSyncPV (const casCtx &ctxIn, const pvInfo &setup) :
	exPV (ctxIn, setup)
{
}

//
// exSyncPV::~exSyncPV()
//
exSyncPV::~exSyncPV() 
{
}

//
// exSyncPV::write()
//
caStatus exSyncPV::write (const casCtx &, gdd &valueIn)
{
	return this->update (valueIn);
}

//
// exSyncPV::read()
//
caStatus exSyncPV::read (const casCtx &, gdd &protoIn)
{
	return exServer::read(*this, protoIn);
}

