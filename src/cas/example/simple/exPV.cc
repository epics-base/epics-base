//
// Example EPICS CA server
//

#include "exServer.h"
#include "gddApps.h"

//
// static data for exPV
//
char exPV::hasBeenInitialized = 0;
gddAppFuncTable<exPV> exPV::ft;
osiTime exPV::currentTime;

//
// exPV::exPV()
//
exPV::exPV (pvInfo &setup, aitBool preCreateFlag, aitBool scanOnIn) : 
	info (setup),
	interest (aitFalse),
	preCreate (preCreateFlag),
	scanOn (scanOnIn)
{
	//
	// no dataless PV allowed
	//
	assert (this->info.getElementCount()>=1u);

	//
	// start a very slow background scan 
	// (we will speed this up to the normal rate when
	// someone is watching the PV)
	//
	if (this->scanOn) {
		this->pScanTimer = 
			new exScanTimer (this->getScanPeriod(), *this);
	}
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
	this->info.unlinkPV();
}

//
// exPV::destroy()
//
// this is replaced by a noop since we are 
// pre-creating most of the PVs during init in this simple server
//
void exPV::destroy()
{
	if (!this->preCreate) {
		delete this;
	}
}

//
// exPV::update()
//
caStatus exPV::update(gdd &valueIn)
{
	unsigned epochSecPast1970 = 7305*86400; /* 1/1/90 20 yr (5 leap) of seconds */
	caServer *pCAS = this->getCAS();
	//
	// gettimeofday() is very slow under sunos4
	//
	osiTime cur (this->currentTime);
	struct timespec t;
	caStatus cas;
 
#	if DEBUG
		printf("Setting %s too:\n", this->info.getName().string());
		valueIn.dump();
#	endif

	cas = this->updateValue (valueIn);
	if (cas || this->pValue==NULL) {
		return cas;
	}

	//
	// this converts from a POSIX time stamp to an EPICS time stamp
	//
	t.tv_sec = (time_t) cur.getSecTruncToLong () - epochSecPast1970;
	t.tv_nsec = cur.getNSecTruncToLong ();
	this->pValue->setTimeStamp(&t);
	this->pValue->setStat (epicsAlarmNone);
	this->pValue->setSevr (epicsSevNone);

	//
	// post a value change event
	//
	if (this->interest==aitTrue && pCAS!=NULL) {
		casEventMask select(pCAS->valueEventMask|pCAS->logEventMask);
		this->postEvent (select, *this->pValue);
	}

	return S_casApp_success;
}

//
// exScanTimer::~exScanTimer ()
//
exScanTimer::~exScanTimer ()
{
	pv.pScanTimer = NULL;
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
	return pv.getScanPeriod();
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

	this->interest = aitTrue;

	if (!this->scanOn) {
		return S_casApp_success;
	}

	//
	// If a slow scan is pending then reschedule it
	// with the specified scan period.
	//
	if (this->pScanTimer) {
		this->pScanTimer->reschedule(this->getScanPeriod());
	}
	else {
		this->pScanTimer = new exScanTimer
				(this->getScanPeriod(), *this);
		if (!this->pScanTimer) {
			errPrintf (S_cas_noMemory, __FILE__, __LINE__,
				"Scan init for %s failed\n", 
				this->info.getName());
			return S_cas_noMemory;
		}
	}

	return S_casApp_success;
}

//
// exPV::interestDelete()
//
void exPV::interestDelete()
{
	this->interest = aitFalse;
	if (this->pScanTimer && this->scanOn) {
		this->pScanTimer->reschedule(this->getScanPeriod());
	}
}

//
// exPV::show()
//
void exPV::show(unsigned level) const
{
	if (level>1u) {
		if (this->pValue!=NULL) {
			printf("exPV: cond=%d\n", this->pValue->getStat());
			printf("exPV: sevr=%d\n", this->pValue->getSevr());
			printf("exPV: value=%f\n", (double) *this->pValue);
		}
		printf("exPV: interest=%d\n", this->interest);
		printf("exPV: pScanTimer=%p\n", this->pScanTimer);
	}
}

//
// exPV::initFT()
//
void exPV::initFT()
{
	if (exPV::hasBeenInitialized) {
			return;
	}

	exPV::ft.installReadFunc ("status", &exPV::getStatus);
	exPV::ft.installReadFunc ("severity", &exPV::getSeverity);
	exPV::ft.installReadFunc ("seconds", &exPV::getSeconds);
	exPV::ft.installReadFunc ("nanoseconds", &exPV::getNanoseconds);
	exPV::ft.installReadFunc ("precision", &exPV::getPrecision);
	exPV::ft.installReadFunc ("graphicHigh", &exPV::getHighLimit);
	exPV::ft.installReadFunc ("graphicLow", &exPV::getLowLimit);
	exPV::ft.installReadFunc ("controlHigh", &exPV::getHighLimit);
	exPV::ft.installReadFunc ("controlLow", &exPV::getLowLimit);
	exPV::ft.installReadFunc ("alarmHigh", &exPV::getHighLimit);
	exPV::ft.installReadFunc ("alarmLow", &exPV::getLowLimit);
	exPV::ft.installReadFunc ("alarmHighWarning", &exPV::getHighLimit);
	exPV::ft.installReadFunc ("alarmLowWarning", &exPV::getLowLimit);
	exPV::ft.installReadFunc ("units", &exPV::getUnits);
	exPV::ft.installReadFunc ("value", &exPV::getValue);
	exPV::ft.installReadFunc ("enums", &exPV::getEnums);

	exPV::hasBeenInitialized = 1;
}

//
// exPV::getStatus()
//
caStatus exPV::getStatus(gdd &value)
{
	if (this->pValue!=NULL) {
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
	if (this->pValue!=NULL) {
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
	if (this->pValue!=NULL) {
		this->pValue->getTimeStamp(&ts);
	}
	else {
		unsigned epochSecPast1970 = 7305*86400; /* 1/1/90 20 yr (5 leap) of seconds */
		osiTime cur(osiTime::getCurrent());
		//
		// this converts from a POSIX time stamp to an EPICS time stamp
		//
		ts.tv_sec = (time_t) cur.getSecTruncToLong () - epochSecPast1970;
		ts.tv_nsec = cur.getNSecTruncToLong ();
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
	aitString str("furlongs", aitStrRefConstImortal);
	units.put(str);
	return S_cas_success;
}

//
// exPV::getEnums()
//
// returns the eneumerated state strings
// for a discrete channel
//
// The PVs in this example are purely analog,
// and therefore this isnt appropriate in an
// analog context ...
//
caStatus exPV::getEnums (gdd &enums)
{
	unsigned nStr = 2;
	aitFixedString *str;

	enums.setDimension(1);
	str = new aitFixedString[nStr];
	if (!str) {
		return S_casApp_noMemory;
	}

	enums.putRef (str,new gddDestructor);

	strncpy (str[0].fixed_string, "off", sizeof(str[0].fixed_string));
	strncpy (str[1].fixed_string, "on", sizeof(str[1].fixed_string));

	enums.setBound (0,0,nStr);

	return S_cas_success;
}

//
// exPV::getValue()
//
caStatus exPV::getValue(gdd &value)
{
	caStatus status;

	if (this->pValue!=NULL) {
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
	return this->ft.read (*this, protoIn);
}

//
// exPV::createChannel()
//
// for access control - optional
//
casChannel *exPV::createChannel (const casCtx &ctx,
		const char * const pUserName, const char * const pHostName)
{
	return new exChannel (ctx);
}


