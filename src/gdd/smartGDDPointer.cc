
//
// Smart Pointer Class for GDD
// ( automatically takes care of referencing / unreferening a gdd each time that it is used )
//
// Author: Jeff Hill
//

#define epicsExportSharedSymbols
#include <smartGddPointer.h>

//
// smartGDDPointer::set()
//
void smartGDDPointer::set (gdd *pNewValue) 
{
	int gddStatus;
	//
	// dont change the ref count (and
	// potentially unref a gdd that we are 
	// still using if the pointer isnt changing
	//
	if (this->pValue==pNewValue) {
		return;
	}
	if (this->pValue!=NULL) {
		gddStatus = this->pValue->unreference();
		assert (!gddStatus);
	}
	this->pValue = pNewValue;
	if (this->pValue!=NULL) {
		gddStatus = this->pValue->reference();
		assert (!gddStatus);
	}
}
