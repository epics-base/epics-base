
#include <math.h>
#include <limits.h>
#include <stdlib.h>

#include "exServer.h"
#include "gddApps.h"

#define myPI 3.14159265358979323846

//
// SUN C++ does not have RAND_MAX yet
//
#if !defined(RAND_MAX)
//
// Apparently SUN C++ is using the SYSV version of rand
//
#if 0
#define RAND_MAX INT_MAX
#else
#define RAND_MAX SHRT_MAX
#endif
#endif

//
// exScalarPV::scan
//
void exScalarPV::scan()
{
	caStatus        status;
	double          radians;
	smartGDDPointer	pDD;
	float           newValue;
	float           limit;
	int				gddStatus;

	//
	// update current time (so we are not required to do
	// this every time that we write the PV which impacts
	// throughput under sunos4 because gettimeofday() is
	// slow)
	//
	this->currentTime = osiTime::getCurrent();

	pDD = new gddScalar (gddAppType_value, aitEnumFloat32);
	if (pDD==NULL) {
		return;
	}

	//
	// smart pointer class manages reference count after this point
	//
	gddStatus = pDD->unreference();
	assert (!gddStatus);

	radians = (rand () * 2.0 * myPI)/RAND_MAX;
	if (this->pValue!=NULL) {
		this->pValue->getConvert(newValue);
	}
	else {
		newValue = 0.0f;
	}
	newValue += (float) (sin (radians) / 10.0);
	limit = (float) this->info.getHopr();
	newValue = tsMin (newValue, limit);
	limit = (float) this->info.getLopr();
	newValue = tsMax (newValue, limit);
	*pDD = newValue;
	status = this->update (*pDD);
	if (status!=S_casApp_success) {
		errMessage (status, "scalar scan update failed\n");
	}
}

//
// exScalarPV::updateValue ()
//
// NOTES:
// 1) This should have a test which verifies that the 
// incoming value in all of its various data types can
// be translated into a real number?
// 2) We prefer to unreference the old PV value here and
// reference the incomming value because this will
// result in each value change events retaining an
// independent value on the event queue.
//
caStatus exScalarPV::updateValue (gdd &valueIn)
{
	//
	// Really no need to perform this check since the
	// server lib verifies that all requests are in range
	//
	if (!valueIn.isScalar()) {
		return S_casApp_outOfBounds;
	}

	this->pValue = &valueIn;

	return S_casApp_success;
}

