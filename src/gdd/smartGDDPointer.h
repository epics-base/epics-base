
//
// Smart Pointer Class for GDD
// ( automatically takes care of referencing / unreferening a gdd each time that it is used )
//
// Author: Jeff Hill
//

#ifndef smartGDDPointer_h
#define smartGDDPointer_h

#include "gdd.h"
#include "shareLib.h"

//
// smartConstGDDPointer
//
// smart pointer class which auto ref/unrefs a const GDD
//
class epicsShareClass smartConstGDDPointer {
public:

	smartConstGDDPointer () :
		pConstValue (0) {}

	smartConstGDDPointer (const gdd &valueIn) :
		pConstValue (&valueIn)
	{
		int gddStatus;
		gddStatus = this->pConstValue->reference ();
		assert (!gddStatus);
	}

	smartConstGDDPointer (const gdd *pValueIn) :
		pConstValue (pValueIn)
	{
		if ( this->pConstValue != NULL ) {
			int gddStatus;
			gddStatus = this->pConstValue->reference ();
			assert (!gddStatus);
		}
	}

	smartConstGDDPointer (const smartConstGDDPointer &ptrIn) :
		pConstValue (ptrIn.pConstValue)
	{
		if ( this->pConstValue != NULL ) {
			int gddStatus;
			gddStatus = this->pConstValue->reference();
			assert(!gddStatus);
		}
	}

	~smartConstGDDPointer ()
	{
		if ( this->pConstValue != NULL ) {
			int gddStatus;
			gddStatus = this->pConstValue->unreference();
			assert (!gddStatus);
		}
	}

	void set (const gdd *pNewValue);

	smartConstGDDPointer operator = (const gdd *rhs) 
	{
		this->set (rhs);
		return *this;
	}

	smartConstGDDPointer operator = (const smartConstGDDPointer &rhs) 
	{
		this->set (rhs.pConstValue);
		return *this;
	}

	const gdd *operator -> () const
	{
		return this->pConstValue;
	}

	const gdd & operator * () const
	{
		return *this->pConstValue;
	}

	bool operator ! () const
	{
        if (this->pConstValue) {
            return false;
        }
        else {
            return true;
        }
	}

    //
    // see Meyers, "more effective C++", for explanation on
    // why conversion to dumb pointer is ill advised here
    //
	//operator const gdd * () const
	//{
	//	return this->pConstValue;
	//}

protected:
    union {
	    gdd *pValue;
	    const gdd *pConstValue;
    };
};

//
// smartGDDPointer
//
// smart pointer class which auto ref/unrefs the GDD
//
class epicsShareClass smartGDDPointer : public smartConstGDDPointer {
public:
    smartGDDPointer () : 
        smartConstGDDPointer () {}

	smartGDDPointer (gdd &valueIn) :
        smartConstGDDPointer (valueIn) {}

	smartGDDPointer (gdd *pValueIn) :
        smartConstGDDPointer (pValueIn) {}

	smartGDDPointer (const smartGDDPointer &ptrIn) :
        smartConstGDDPointer (ptrIn) {}

	void set (gdd *pNewValue) 
    {
        this->smartConstGDDPointer::set (pNewValue);
    }

	smartGDDPointer operator = (gdd *rhs) 
	{
        this->smartGDDPointer::set (rhs);
		return *this;
	}

	smartGDDPointer operator = (const smartGDDPointer &rhs) 
	{
        this->smartGDDPointer::set (rhs.pValue);
		return *this;
	}

	gdd *operator -> () const
	{
		return this->pValue;
	}

	gdd & operator * () const
	{
		return *this->pValue;
	}

    //
    // see Meyers, "more effective C++", for explanation on
    // why conversion to dumb pointer is ill advised here
    //
	//operator gdd * () const
	//{
	//	return this->pValue;
	//}
};

//
// smartConstGDDReference
//
// smart reference class which auto ref/unrefs a const GDD
//
// Notes: 
// 1) must be used with "->" operator and not "." operator
// 2) must be used with "*" operator as an L-value
//
class epicsShareClass smartConstGDDReference {
public:

	smartConstGDDReference (const gdd *pValueIn) :
		pConstValue (pValueIn)
	{
        assert (this->pConstValue);
		int gddStatus;
		gddStatus = this->pConstValue->reference ();
		assert (!gddStatus);
	}

	smartConstGDDReference (const gdd &valueIn) :
		pConstValue (&valueIn)
	{
		int gddStatus;
		gddStatus = this->pConstValue->reference ();
		assert (!gddStatus);
	}

	smartConstGDDReference (const smartConstGDDReference &refIn) :
		pConstValue (refIn.pConstValue)
	{
		int gddStatus;
		gddStatus = this->pConstValue->reference();
		assert (!gddStatus);
	}

	//smartConstGDDReference (const smartConstGDDPointer &ptrIn) :
	//	pConstValue (ptrIn.pConstValue)
	//{
    //    assert (this->pConstValue)
	//	int gddStatus;
	//	gddStatus = this->pConstValue->reference();
	//	assert (!gddStatus);
	//}

	~smartConstGDDReference ()
	{
		int gddStatus;
		gddStatus = this->pConstValue->unreference();
		assert (!gddStatus);
	}

	const gdd *operator -> () const
	{
		return this->pConstValue;
	}

	const gdd & operator * () const
	{
		return *this->pConstValue;
	}
protected:
    union {
	    gdd *pValue;
	    const gdd *pConstValue;
    };
};

//
// smartGDDReference
//
// smart reference class which auto ref/unrefs a const GDD
//
// Notes: 
// 1) must be used with "->" operator and not "." operator
// 2) must be used with "*" operator as an L-value
//
class epicsShareClass smartGDDReference : public smartConstGDDReference {
public:

	smartGDDReference (gdd *pValueIn) :
      smartConstGDDReference (pValueIn) {}

	smartGDDReference (gdd &valueIn) :
        smartConstGDDReference (valueIn) {}

	smartGDDReference (const smartGDDReference &refIn) :
        smartConstGDDReference (refIn) {}

	//smartGDDReference (const smartGDDPointer &ptrIn) :
	//	smartConstGDDReference (ptrIn)

	gdd *operator -> () const
	{
		return this->pValue;
	}

	gdd & operator * () const
	{
		return *this->pValue;
	}
};

#endif // smartGDDPointer_h

