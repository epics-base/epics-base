//
// Example EPICS CA server
//

#include <exServer.h>
#include <gddApps.h>

const double myPI = 3.14159265358979323846;

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
	// load initial value
	//
	this->scanPV();
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
// exPV::scanPV();
//
void exPV::scanPV()
{
	caStatus	status;
	double		radians;
	gdd		*pDD;
	float		newValue;
	float		limit;
	caServer	*pCAS = this->getCAS();

	if (!pCAS) {
		return;
	}

	//
	// update current time (so we are not required to do
	// this every time that we write the PV which impacts
	// throughput under sunos4 because gettimeofday() is
	// slow)
	//
	this->currentTime = osiTime::getCurrent();

	pDD = new gddScalar (gddAppType_value, aitEnumFloat32);
	if (!pDD) {
		return;
	}

	radians = (rand () * 2.0 * myPI)/RAND_MAX;
	if (this->pValue) {
		this->pValue->getConvert(newValue);
	}
	else {
		newValue = 0.0f;
	}
	newValue += (float) (sin (radians) / 10.0);
	limit = (float) this->info.getHopr();
	newValue = min (newValue, limit);
	limit = (float) this->info.getLopr();
	newValue = max (newValue, limit);
	*pDD = newValue;
	pDD->setStat (epicsAlarmNone);
	pDD->setSevr (epicsSevNone);
	status = this->update (*pDD);
	if (status) {
		errMessage (status, "scan update failed\n");
	}

	pDD->unreference();
}

//
// exScanTimer::expire ()
//
void exScanTimer::expire ()
{
        pv.scanPV();
}

//
// exScanTimer::again()
//
osiBool exScanTimer::again()
{
	return osiTrue;
}

//
// exScanTimer::delay()
//
const osiTime exScanTimer::delay()
{
	return pv.getScanRate();
}

//
// exPV::update ()
//
caStatus exPV::update(gdd &valueIn)
{
	gdd *pNewValue;
	caServer *pCAS = this->getCAS();
	//
	// gettimeofday() is very slow under sunos4
	//
	osiTime cur (this->currentTime);
	struct timespec t;
	gddStatus gdds;


	if (!pCAS) {
		return S_casApp_noSupport; 
	}

#	if DEBUG
		printf("%s = %f\n", 
			this->info.getName().string, valueIn);
#	endif

	if (valueIn.isScalar()) {
		pNewValue = &valueIn;
		pNewValue->reference();
	}
	else {
		//
		// this does not modify the current value 
		// (because it may be referenced in the event queue)
		//
		pNewValue = new gddScalar (gddAppType_value, aitEnumFloat32);
		if (!pNewValue) {
			return S_casApp_noMemory;
		}

		gdds = gddApplicationTypeTable::
			app_table.smartCopy(pNewValue, &valueIn);
		if (gdds) {
			pNewValue->unreference();
			return S_cas_noConvert;
		}

		pNewValue->setStat (epicsAlarmNone);
		pNewValue->setSevr (epicsSevNone);
	}

	cur.get (t.tv_sec, t.tv_nsec);
	pNewValue->setTimeStamp(&t);

	//
	// release old value and replace it
	// with the new one
	//
	if (this->pValue) {
		this->pValue->unreference();
	}
	this->pValue = pNewValue;

	if (this->interest==aitTrue) {
		casEventMask select(pCAS->valueEventMask|pCAS->logEventMask);
		this->postEvent (select, *this->pValue);
	}

	return S_casApp_success;
}


//
// exPV::bestExternalType()
//
aitEnum exPV::bestExternalType()
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
	static aitString str("@#$%");
	units.put(str);
	return S_cas_success;
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

