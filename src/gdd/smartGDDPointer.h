
//
// Smart Pointer Class for GDD
// ( automatically takes care of referencing / unreferening a gdd each time that it is used )
//
// Author: Jeff Hill
//

#ifndef smartGDDPointer_h
#define smartGDDPointer_h

//
// smartGDDPointer
//
// smart pointer class which auto ref/unrefs the GDD
//
class smartGDDPointer {
public:

	smartGDDPointer () :
		pValue (0) {}

	smartGDDPointer (gdd &valueIn) :
		pValue (&valueIn)
	{
		int gddStatus;
		gddStatus = this->pValue->reference();
		assert(!gddStatus);
	}

	smartGDDPointer (gdd *pValueIn) :
		pValue (pValueIn)
	{
		if (this->pValue!=NULL) {
			int gddStatus;
			gddStatus = this->pValue->reference();
			assert(!gddStatus);
		}
	}

	smartGDDPointer (const smartGDDPointer &ptrIn) :
		pValue (ptrIn.pValue)
	{
		if (this->pValue!=NULL) {
			int gddStatus;
			gddStatus = this->pValue->reference();
			assert(!gddStatus);
		}
	}

	~smartGDDPointer ()
	{
		if (this->pValue!=NULL) {
			int gddStatus;
			gddStatus = this->pValue->unreference();
			assert (!gddStatus);
		}
	}

	void set (gdd *pNewValue) 
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

	smartGDDPointer operator = (gdd *rhs) 
	{
		set (rhs);
		return *this;
	}

	smartGDDPointer operator = (smartGDDPointer &rhs) 
	{
		set (rhs.pValue);
		return *this;
	}
#if 0
	int operator == (const smartGDDPointer &rhs) const
	{
		return this->pValue==rhs.pValue;
	}

	int operator != (const smartGDDPointer &rhs) const
	{
		return this->pValue!=rhs.pValue;
	}
#endif
	gdd *operator -> () const
	{
		return this->pValue;
	}

	gdd & operator * () const
	{
		return *this->pValue;
	}

	operator gdd * () const
	{
		return this->pValue;
	}

private:
	gdd *pValue;
};

#endif // smartGDDPointer_h
