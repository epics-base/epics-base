
//
// Smart Pointer Class for GDD
// ( automatically takes care of referencing / unreferening a gdd each time that it is used )
//
// Author: Jeff Hill
//

#define epicsExportSharedSymbols
#include "smartGDDPointer.h"

//
// smartGDDPointer::set()
//
void smartConstGDDPointer::set (const gdd *pNewValue) 
{
	int gddStatus;
	//
	// dont change the ref count (and
	// potentially unref a gdd that we are 
	// still using if the pointer isnt changing
	//
	if ( this->pConstValue == pNewValue ) {
		return;
	}
	if ( this->pConstValue != NULL ) {
		gddStatus = this->pConstValue->unreference();
		assert ( ! gddStatus );
	}
	this->pConstValue = pNewValue;
	if ( this->pConstValue != NULL ) {
		gddStatus = this->pConstValue->reference();
		assert ( ! gddStatus );
	}
}
