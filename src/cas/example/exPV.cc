//
// Example EPICS CA server
//

#include <exServer.h>

const double myPI = 3.14159265358979323846;

//
// exPV::exPV()
//
exPV::exPV (const casCtx &ctxIn, const pvInfo &setup) : 
	pValue(NULL),
	pScanTimer(NULL),
	info(setup),
	casPV(ctxIn, setup.getName().String()),
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
		this->pValue->Unreference();
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

	pDD = new gddAtomic (gddAppType_value, aitEnumFloat32);
	if (!pDD) {
		return;
	}

	radians = (rand () * 2.0 * myPI)/RAND_MAX;
	if (this->pValue) {
		this->pValue->GetConvert(newValue);
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
	status = this->update (*pDD);
	if (status) {
		errMessage (status, "scan update failed\n");
	}

	pDD->Unreference();
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
	osiTime cur (osiTime::getCurrent());
	struct timespec t;
	gddStatus gdds;

	if (!pCAS) {
		return S_casApp_noSupport; 
	}

	//
	// this does not modify the current value 
	// (because it may be referenced in the event queue)
	//
	pNewValue = new gdd (gddAppType_value, aitEnumFloat32);
	if (!pNewValue) {
		return S_casApp_noMemory;
	}

#	if DEBUG
		printf("%s = %f\n", this->info.getName().String, valueIn);
#	endif

	gdds = gddApplicationTypeTable::
		app_table.SmartCopy(pNewValue, &valueIn);
	if (gdds) {
		pNewValue->Unreference();
		return S_cas_noConvert;
	}

	cur.get (t.tv_sec, t.tv_nsec);
	pNewValue->SetTimeStamp(&t);

	pNewValue->SetStat (epicsAlarmNone);
	pNewValue->SetSevr (epicsSevNone);

	//
	// release old value and replace it
	// with the new one
	//
	if (this->pValue) {
		this->pValue->Unreference();
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
	return aitEnumFloat64;
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
				this->info.getName().String());
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
			printf("exPV: cond=%d\n", this->pValue->GetStat());
			printf("exPV: sevr=%d\n", this->pValue->GetSevr());
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
		value.PutConvert(this->pValue->GetStat());
	}
	else {
		value.PutConvert(epicsAlarmUDF);
	}
	return S_cas_success;
}

//
// exPV::getSeverity()
//
caStatus exPV::getSeverity(gdd &value)
{
	if (this->pValue) {
		value.PutConvert(this->pValue->GetSevr());
	}
	else {
		value.PutConvert(epicsSevInvalid);
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
		this->pValue->GetTimeStamp(&ts);
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
	value.PutConvert (sec);
	return S_cas_success;
}

//
// exPV::getNanoseconds()
//
caStatus exPV::getNanoseconds(gdd &value)
{
	aitUint32 nsec (this->getTS().tv_nsec);
	value.PutConvert (nsec);
	return S_cas_success;
}

//
// exPV::getPrecision()
//
caStatus exPV::getPrecision(gdd &prec)
{
	prec.PutConvert(4u);
	return S_cas_success;
}

//
// exPV::getHighLimit()
//
caStatus exPV::getHighLimit(gdd &value)
{
	value.PutConvert(info.getHopr());
	return S_cas_success;
}

//
// exPV::getLowLimit()
//
caStatus exPV::getLowLimit(gdd &value)
{
	value.PutConvert(info.getLopr());
	return S_cas_success;
}

//
// exPV::getUnits()
//
caStatus exPV::getUnits(gdd &units)
{
	static aitString str("@#$%");
	units.PutRef(str);
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
			app_table.SmartCopy(&value, this->pValue);
		if (gdds) {
			status = S_cas_noConvert;	
		}
		else {
			status = S_cas_success;
		}
	}
	else {
		status = S_cas_noMemory;
	}
	return status;
}

