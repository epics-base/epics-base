
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
// exVectorPV::maxDimension()
//
unsigned exVectorPV::maxDimension() const
{
	return 1u;
}

//
// exVectorPV::maxBound()
//
aitIndex exVectorPV::maxBound (unsigned dimension) const
{
	if (dimension==0u) {
		return this->info.getElementCount();
	}
	else {
		return 0u;
	}
}

//
// exVectorPV::scan
//
void exVectorPV::scan()
{
        caStatus        status;
        double          radians;
        gdd             *pDD;
	aitFloat32 	*pF, *pFE, *pCF;
        float           newValue;
        float           limit;
 
        //
        // update current time (so we are not required to do
        // this every time that we write the PV which impacts
        // throughput under sunos4 because gettimeofday() is
        // slow)
        //
        this->currentTime = osiTime::getCurrent();
 
	pDD = new gddAtomic (gddAppType_value, aitEnumFloat32, 
			1u, this->info.getElementCount());
	if (pDD==NULL) {
		return;
	}
 
	//
	// allocate array buffer
	//
	pF = new aitFloat32 [this->info.getElementCount()];
	if (!pF) {
		pDD->unreference();
	}

	//
	// install the buffer into the DD
	// (do this before we increment pF)
	//
	*pDD = pF;

	//
	// double check for reasonable bounds on the
	// current value
	//
	pCF=NULL;
	if (this->pValue) {
		if (this->pValue->dimension()==1u) {
			const gddBounds *pB = this->pValue->getBounds();
			if (pB[0u].size()==this->info.getElementCount()) {
				pCF = *pDD;
			}
		}
	}

	pFE = &pF[this->info.getElementCount()];
	while (pF<pFE) {
		radians = (rand () * 2.0 * myPI)/RAND_MAX;
		if (pCF) {
			newValue = *pCF++;
		}
		else {
			newValue = 0.0f;
		}
		newValue += (float) (sin (radians) / 10.0);
		limit = (float) this->info.getHopr();
		newValue = tsMin (newValue, limit);
		limit = (float) this->info.getLopr();
		newValue = tsMax (newValue, limit);
		*(pF++) = newValue;
	}

        status = this->update (*pDD);
        if (status) {
                errMessage (status, "scan update failed\n");
        }
 
        pDD->unreference();
}

//
// exVectorPV::updateValue ()
//
// NOTES:
// 1) This should have a test which verifies that the
// incoming value in all of its various data types can
// be translated into a real number?
// 2) We prefer to unreference the old PV value here and
// reference the incomming value because this will
// result in value change events each retaining an
// independent value on the event queue. With large arrays
// this may result in too much memory consumtion on
// the event queue.
//
caStatus exVectorPV::updateValue(gdd &valueIn)
{
	enum {replace, dontReplace} replFlag = dontReplace;
        gddStatus gdds;
        gdd *pNewValue;
 
	//
	// Check bounds of incoming request
	// (and see if we are replacing all elements -
	// replaceOk==TRUE)
	//
	// Perhaps much of this is unnecessary since the
	// server lib checks the bounds of all requests
	//
	if (valueIn.isAtomic()) {
		if (valueIn.dimension()!=1u) {
			return S_casApp_badDimension;
		}
		const gddBounds* pb = valueIn.getBounds();
		if (pb[0u].first()!=0u) {
			return S_casApp_outOfBounds;
		}
		if (pb[0u].size()==this->info.getElementCount()) {
			replFlag = replace;
		}
		else if (pb[0u].size()>this->info.getElementCount()) {
			return S_casApp_outOfBounds;
		}
	}
	else if (!valueIn.isScalar()) {
		//
		// no containers
		//
		return S_casApp_outOfBounds;
	}

	if (replFlag==replace) {
		//
		// replacing all elements is efficient
		//
		valueIn.reference();
		pNewValue = &valueIn;
	}
	else {
		aitFloat32 *pF, *pFE;

		//
		// Create a new array data descriptor
		// (so that old values that may be referenced on the
		// event queue are not replaced)
		//
		pNewValue = new gddAtomic (gddAppType_value, aitEnumFloat32, 
				1u, this->info.getElementCount());
		if (pNewValue==NULL) {
			return S_casApp_noMemory;
		}

		//
		// copy over the old values if they exist
		// (or initialize all elements to zero)
		//
		if (this->pValue) {
			gdds = pNewValue->copy(this->pValue);
			if (gdds) {
				pNewValue->unreference();
				return S_cas_noConvert;
			}
		}
		else {
			//
			// allocate array buffer
			//
			pF = new aitFloat32 [this->info.getElementCount()];
			if (!pF) {
				pNewValue->unreference();
				return S_casApp_noMemory;
			}

			//
			// Install (and initialize) array buffer
			// if no old values exist
			//
			pFE = &pF[this->info.getElementCount()];
			while (pF<pFE) {
				*(pF++) = 0.0;
			}
			*pNewValue = pF;
		}

		//
		// insert the values that they are writing
		//
		gdds = pNewValue->put(&valueIn);
		if (gdds) {
			pNewValue->unreference();
			return S_cas_noConvert;
		}
	}

	if (this->pValue) {
		this->pValue->unreference();
	}
	this->pValue = pNewValue;

        return S_casApp_success;
}

