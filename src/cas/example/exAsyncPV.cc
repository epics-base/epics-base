
//
// Example EPICS CA server
//

#include <exServer.h>

//
// exAsyncPV::maxSimultAsyncOps()
// (virtual replacement for the default)
//
unsigned exAsyncPV::maxSimultAsyncOps () const
{
        return 500u;
}

//
// exAsyncPV::read()
// (virtual replacement for the default)
//
caStatus exAsyncPV::read (const casCtx &ctx, gdd &valueIn)
{
	exAsyncIO	*pIO;
	
	pIO = new exAsyncReadIO(ctx, *this, valueIn);
	if (!pIO) {
		return S_casApp_noMemory;
	}

        return S_casApp_asyncCompletion;
}

//
// exAsyncPV::write()
// (virtual replacement for the default)
//
caStatus exAsyncPV::write (const casCtx &ctx, gdd &valueIn)
{
	exAsyncIO	*pIO;
	
	pIO = new exAsyncWriteIO(ctx, *this, valueIn);
	if (!pIO) {
		return S_casApp_noMemory;
	}

        return S_casApp_asyncCompletion;
}

//
// exAsyncWriteIO::expire()
// (a virtual function that runs when the base timer expires)
//
void exAsyncWriteIO::expire() 
{
	caStatus status;
	status = this->pv.update(*this->getValuePtr());
	this->clrValue();
	this->postIOCompletion (status);
}

//
// exAsyncReadIO::expire()
// (a virtual function that runs when the base timer expires)
//
void exAsyncReadIO::expire()
{
	caStatus status;
	gdd *pValue = this->getValuePtr();

	//
	// map between the prototype in and the
	// current value
	//
	status = exServer::read(this->pv, *pValue);

	//
	// post IO completion
	//
	this->postIOCompletion(status);
}

//
// exAsyncExistIO::expire()
// (a virtual function that runs when the base timer expires)
//
void exAsyncExistIO::expire()
{
	//
	// post IO completion
	//
	this->postIOCompletion(S_cas_success);
}

