//
// fileDescriptorManager.process(delay);
// (the name of the global symbol has leaked in here)
//

//
// Example EPICS CA server
//

#include <exServer.h>

const pvInfo exServer::pvList[] = {
	pvInfo (1.0e-1, "jane", 10.0f, 0.0f, excasIoSync, 1u),
	pvInfo (2.0, "fred", 10.0f, -10.0f, excasIoSync, 1u),
	pvInfo (1.0e-1, "janet", 10.0f, 0.0f, excasIoAsync, 1u),
	pvInfo (2.0, "freddy", 10.0f, -10.0f, excasIoAsync, 1u),
	pvInfo (2.0, "alan", 10.0f, -10.0f, excasIoSync, 100u),
	pvInfo (20.0, "albert", 10.0f, -10.0f, excasIoSync, 1000u)
};

//
// static data for exServer
//
gddAppFuncTable<exPV> exServer::ft;

//
// exServer::exServer()
//
exServer::exServer(unsigned pvMaxNameLength, unsigned pvCountEstimate, 
	unsigned maxSimultaneousIO) :
	caServer(pvMaxNameLength, pvCountEstimate, maxSimultaneousIO)
{
	ft.installReadFunc("status",exPV::getStatus);
	ft.installReadFunc("severity",exPV::getSeverity);
	ft.installReadFunc("seconds",exPV::getSeconds);
	ft.installReadFunc("nanoseconds",exPV::getNanoseconds);
	ft.installReadFunc("precision",exPV::getPrecision);
	ft.installReadFunc("graphicHigh",exPV::getHighLimit);
	ft.installReadFunc("graphicLow",exPV::getLowLimit);
	ft.installReadFunc("controlHigh",exPV::getHighLimit);
	ft.installReadFunc("controlLow",exPV::getLowLimit);
	ft.installReadFunc("alarmHigh",exPV::getHighLimit);
	ft.installReadFunc("alarmLow",exPV::getLowLimit);
	ft.installReadFunc("alarmHighWarning",exPV::getHighLimit);
	ft.installReadFunc("alarmLowWarning",exPV::getLowLimit);
	ft.installReadFunc("units",exPV::getUnits);
	ft.installReadFunc("value",exPV::getValue);
	ft.installReadFunc("enums",exPV::getEnums);
}

//
// exServer::pvExistTest()
//
pvExistReturn exServer::pvExistTest(const casCtx &ctxIn, const char *pPVName)
{
	const pvInfo	*pPVI;

	pPVI = exServer::findPV(pPVName);
	if (pPVI) {
		if (pPVI->getIOType()==excasIoAsync) {
			exAsyncExistIO	*pIO;
			pIO = new exAsyncExistIO(*pPVI, ctxIn);
			if (pIO) {
				return pvExistReturn(S_casApp_asyncCompletion);
			}
			else {
				return pvExistReturn(S_casApp_noMemory);
			}
		}

		const char *pName = pPVI->getName();
		return pvExistReturn(S_casApp_success, pName);
	}

	return pvExistReturn(S_casApp_pvNotFound);
}

//
// findPV()
//
const pvInfo *exServer::findPV(const char *pName)
{
	const pvInfo *pPVI;
	const pvInfo *pPVAfter = 
		&exServer::pvList[NELEMENTS(exServer::pvList)];

	for (pPVI = exServer::pvList; pPVI < pPVAfter; pPVI++) {
		if (strcmp (pName, pPVI->getName().string()) == '\0') {
			return pPVI;
		}
	}
	return NULL;
}

//
// exServer::createPV()
//
casPV *exServer::createPV (const casCtx &ctxIn, const char *pPVName)
{
	const pvInfo	*pInfo;
	exPV		*pPV;

	pInfo = exServer::findPV(pPVName);
	if (!pInfo) {
		return NULL;
	}

	//
	// create an instance of the appropriate class
	// depending on the io type and the number
	// of elements
	//
	if (pInfo->getElementCount()==1u) {
		switch (pInfo->getIOType()){
		case excasIoSync:
			pPV = new exScalarPV (ctxIn, *pInfo);
			break;
		case excasIoAsync:
			pPV = new exAsyncPV (ctxIn, *pInfo);
			break;
		default:
			pPV = NULL;
			break;
		}
	}
	else {
		pPV = new exVectorPV (ctxIn, *pInfo);
	}
	
	//
	// load initial value (this is not done in
	// the constructor because the base class's
	// pure virtual function would be called)
	//
	if (pPV) {
		pPV->scan();
	}

	return pPV;
}

//
// exServer::show()
//
void exServer::show (unsigned level)
{
	//
	// server tool specific show code goes here
	//

	//
	// print information about ca server libarary
	// internals
	//
	this->caServer::show(level);
}

//
// this is a noop that postpones the timer expiration
// destroy so the exAsyncIO class will hang around until the
// casAsyncIO::destroy() is called
//
void exOSITimer::destroy()
{
}

//
// exAsyncExistIO::expire()
// (a virtual function that runs when the base timer expires)
//
void exAsyncExistIO::expire()
{
	const char *pName = pvi.getName();

        //
        // post IO completion
        //
        this->postIOCompletion (pvExistReturn(S_cas_success, pName));
}

//
// exAsyncExistIO::name()
//
const char *exAsyncExistIO::name() const
{
	return "exAsyncExistIO";
}

