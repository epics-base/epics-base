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
// special gddDestructor guarantees same form of new and delete
//
class exFixedStringDestructor: public gddDestructor {
	virtual void run (void *);
};

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
	caServer *pCAS = this->getCAS();
	caStatus cas;
 
#	if DEBUG
		printf("Setting %s too:\n", this->info.getName().string());
		valueIn.dump();
#	endif

	cas = this->updateValue (valueIn);
	if (cas || this->pValue==NULL) {
		return cas;
	}

	aitTimeStamp ts = this->currentTime;
	this->pValue->setTimeStamp (&ts);
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
bool exScanTimer::again() const
{
	return true;
}

//
// exScanTimer::delay()
//
double exScanTimer::delay() const
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

	//
	// time stamp, status, and severity are extracted from the
	// GDD associated with the "value" application type.
	//
	exPV::ft.installReadFunc ("value", &exPV::getValue);
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
	exPV::ft.installReadFunc ("enums", &exPV::getEnums);

	exPV::hasBeenInitialized = 1;
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
	exFixedStringDestructor *pDes;

	enums.setDimension(1);
	str = new aitFixedString[nStr];
	if (!str) {
		return S_casApp_noMemory;
	}

	pDes = new exFixedStringDestructor;
	if (!pDes) {
		delete [] str;
		return S_casApp_noMemory;
	}

	enums.putRef (str, pDes);

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
		const char * const /* pUserName */, 
		const char * const /* pHostName */)
{
	return new exChannel (ctx);
}

//
// exFixedStringDestructor::run()
//
// special gddDestructor guarantees same form of new and delete
//
void exFixedStringDestructor::run (void *pUntyped)
{
	aitFixedString *ps = (aitFixedString *) pUntyped;
	delete [] ps;
}

