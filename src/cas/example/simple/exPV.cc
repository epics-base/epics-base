//
// Example EPICS CA server
//

#include <exServer.h>
#include <gddApps.h>

osiTime exPV::currentTime;

//
// exPV::exPV()
//
exPV::exPV (const casCtx &ctxIn, const pvInfo &setup) : 
	pValue(NULL),
	pScanTimer(NULL),
	info(setup),
	casPV(ctxIn, setup.getName().string()),
	interest(aitFalse)
{
	//
	// no dataless PV allowed
	//
	assert (this->info.getElementCount()>=1u);
}

//
// exPV::~exPV()
//
exPV::~exPV() 
{
	if (this->pScanTimer) {
		delete this->pScanTimer;
		this->pScanTimer = NULL;
	}
	if (this->pValue) {
		this->pValue->unreference();
		this->pValue = NULL;
	}
}

//
// exPV::update()
//
caStatus exPV::update(gdd &valueIn)
{
        caServer *pCAS = this->getCAS();
        //
        // gettimeofday() is very slow under sunos4
        //
        osiTime cur (this->currentTime);
        struct timespec t;
	caStatus cas;
 
        if (!pCAS) {
                return S_casApp_noSupport;
        }
 
#       if DEBUG
                printf("Setting %s too:\n", this->info.getName().string());
                valueIn.dump();
#       endif

	cas = this->updateValue (valueIn);
	if (cas || !this->pValue) {
		return cas;
	}

        cur.get (t.tv_sec, t.tv_nsec);
        this->pValue->setTimeStamp(&t);
	this->pValue->setStat (epicsAlarmNone);
	this->pValue->setSevr (epicsSevNone);
	
	//
	// post a value change event
	//
        if (this->interest==aitTrue) {
                casEventMask select(pCAS->valueEventMask|pCAS->logEventMask);
                this->postEvent (select, *this->pValue);
        }
 
        return S_casApp_success;
}

//
// exScanTimer::expire ()
//
void exScanTimer::expire ()
{
        pv.scan();
}

//
// exScanTimer::again()
//
osiBool exScanTimer::again() const
{
	return osiTrue;
}

//
// exScanTimer::delay()
//
const osiTime exScanTimer::delay() const
{
	return pv.getScanRate();
}

//
// exScanTimer::name()
//
const char *exScanTimer::name() const
{
	return "exScanTimer";
}

//
// exPV::bestExternalType()
//
aitEnum exPV::bestExternalType() const
{
	return aitEnumFloat32;
}

//
// exPV::interestRegister()
//
caStatus exPV::interestRegister()
{
	caServer	*pCAS = this->getCAS();

	if (!pCAS) {
		return S_casApp_success;
	}

	if (!this->pScanTimer) {
		this->pScanTimer = new exScanTimer
				(this->info.getScanRate(), *this);
		if (!this->pScanTimer) {
			errPrintf (S_cas_noMemory, __FILE__, __LINE__,
				"Scan init for %s failed\n", 
				this->info.getName().string());
			return S_cas_noMemory;
		}
	}

	this->interest = aitTrue;

	return S_casApp_success;
}

//
// exPV::interestDelete()
//
void exPV::interestDelete()
{
	if (this->pScanTimer) {
		delete this->pScanTimer;
		this->pScanTimer = NULL;
	}
	this->interest = aitFalse;
}

//
// exPV::show()
//
void exPV::show(unsigned level) 
{
	if (level>1u) {
		if (this->pValue) {
			printf("exPV: cond=%d\n", this->pValue->getStat());
			printf("exPV: sevr=%d\n", this->pValue->getSevr());
			printf("exPV: value=%f\n", (double) *this->pValue);
		}
		printf("exPV: interest=%d\n", this->interest);
		printf("exPV: pScanTimer=%x\n", (unsigned) this->pScanTimer);
	}
}

//
// exPV::getStatus()
//
caStatus exPV::getStatus(gdd &value)
{
	if (this->pValue) {
		value.put(this->pValue->getStat());
	}
	else {
		value.put((aitUint16)epicsAlarmUDF);
	}
	return S_cas_success;
}

//
// exPV::getSeverity()
//
caStatus exPV::getSeverity(gdd &value)
{
	if (this->pValue) {
		value.put(this->pValue->getSevr());
	}
	else {
		value.put((aitUint16)epicsSevInvalid);
	}
	return S_cas_success;
}

//
// exPV::getTS()
//
inline aitTimeStamp exPV::getTS()
{
	aitTimeStamp ts;
	if (this->pValue) {
		this->pValue->getTimeStamp(&ts);
	}
	else {
		osiTime cur(osiTime::getCurrent());
		cur.get(ts.tv_sec, ts.tv_nsec);
	}
	return ts;
}

//
// exPV::getSeconds()
//
caStatus exPV::getSeconds(gdd &value)
{
	aitUint32 sec (this->getTS().tv_sec);
	value.put(sec);
	return S_cas_success;
}

//
// exPV::getNanoseconds()
//
caStatus exPV::getNanoseconds(gdd &value)
{
	aitUint32 nsec (this->getTS().tv_nsec);
	value.put(nsec);
	return S_cas_success;
}

//
// exPV::getPrecision()
//
caStatus exPV::getPrecision(gdd &prec)
{
	prec.put(4u);
	return S_cas_success;
}

//
// exPV::getHighLimit()
//
caStatus exPV::getHighLimit(gdd &value)
{
	value.put(info.getHopr());
	return S_cas_success;
}

//
// exPV::getLowLimit()
//
caStatus exPV::getLowLimit(gdd &value)
{
	value.put(info.getLopr());
	return S_cas_success;
}

//
// exPV::getUnits()
//
caStatus exPV::getUnits(gdd &units)
{
	static aitString str("furlongs");
	units.put(str);
	return S_cas_success;
}

//
// exPV::getEnums()
//
caStatus exPV::getEnums(gdd &)
{
	return S_cas_noConvert;
}

//
// exPV::getValue()
//
caStatus exPV::getValue(gdd &value)
{
	caStatus status;

	if (this->pValue) {
		gddStatus gdds;

		gdds = gddApplicationTypeTable::
			app_table.smartCopy(&value, this->pValue);
		if (gdds) {
			status = S_cas_noConvert;	
		}
		else {
			status = S_cas_success;
		}
	}
	else {
		status = S_casApp_undefined;
	}
	return status;
}

//
// exPV::write()
// (synchronous default)
//
caStatus exPV::write (const casCtx &, gdd &valueIn)
{
        return this->update (valueIn);
}
 
//
// exPV::read()
// (synchronous default)
//
caStatus exPV::read (const casCtx &, gdd &protoIn)
{
        return exServer::read(*this, protoIn);
}

