/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 * History
 * $Log$
 *
 */

#include <gddAppFuncTable.h>

//
// gddAppFuncTable::installReadFunc()
//
// The 2nd parameter has type "gddAppReadFunc" which is
// a ptr to member function. The member function should
// be declared as follows:
//
// gddAppFuncTableStatus PV::memberFunction(gdd &value);
//
// The typedef is not used here because of portability 
// problems resulting from compiler weaknesses
//
template <class PV>
gddAppFuncTableStatus gddAppFuncTable<PV>::installReadFunc(
	const unsigned type, 
	gddAppFuncTableStatus (PV::*pMFuncIn)(gdd &value))
{
	//
	// Attempt to expand the table if the app type will not fit 
	//
	if (type>=this->appTableNElem) {
		this->newTbl(type);
		if (type>=this->appTableNElem) {
			return S_gddAppFuncTable_noMemory;
		}
	}
	this->pMFuncRead[type]=pMFuncIn;
	return S_gddAppFuncTable_Success;
}

//
// installReadFunc()
//
// The 2nd parameter has type "gddAppReadFunc" which is
// a ptr to member function. The member function should
// be declared as follows:
//
// gddAppFuncTableStatus PV::memberFunction(gdd &value);
//
template <class PV>
gddAppFuncTableStatus gddAppFuncTable<PV>::installReadFunc(
	const char * const pName, 
	gddAppFuncTableStatus (PV::*pMFuncIn)(gdd &value))
{
	aitUint32 type;
	gddStatus rc;

	rc = gddApplicationTypeTable::
		app_table.registerApplicationType (pName, type);
	if (rc!=0 && rc!=gddErrorAlreadyDefined) {
		printf(
"at gdd lib limit => read of PV attribute \"%s\" will fail\n", pName);		
		return S_gddAppFuncTable_gddLimit;
	}
#	ifdef DEBUG
		printf("installing PV attribute %s = %d\n", pName, type);		
#	endif
	return this->installReadFunc(type, pMFuncIn);
}

//
// gddAppFuncTable<PV>::newTbl() 
// 
// The total number of application tags to manage should be
// hidden from the application 
//
// The typedef is not used here because of portability 
// problems resulting from compiler weaknesses
//
template <class PV> 
void gddAppFuncTable<PV>::newTbl(unsigned newApplTypeMax) 
{
	gddAppReadFunc *pMNewFuncTbl;
	unsigned maxApp;
	unsigned i;

	if (this->appTableNElem>newApplTypeMax) {
		return;
	}
	maxApp = newApplTypeMax+(1u<<6u);
#	ifdef _MSC_VER
		//
		//      Right now all MS Visual C++ compilers allocate the
		//      wrong amount of memory (i.e. too little)
		//      for member function pointers,
		//      only explicit calculation via sizeof() works.
		//      For future versions this may become "if _MSC_VER < ???"...
		//
		pMNewFuncTbl = (gddAppReadFunc *)
			new char[sizeof(gddAppReadFunc) * maxApp];
#	else
        	pMNewFuncTbl = new gddAppReadFunc[maxApp];
#	endif
	if (pMNewFuncTbl) {
		for (i=0u; i<maxApp; i++) {
			if (i<this->appTableNElem) {
				pMNewFuncTbl[i] = this->pMFuncRead[i];
			}
			else {
				//
				// some versions of NULL include (void *) cast
				// (so I am using vanilla zero here) 
				//
				pMNewFuncTbl[i] = 0; 
			}
		}
		if (this->pMFuncRead) {
			delete [] this->pMFuncRead;
		}
		this->pMFuncRead = pMNewFuncTbl;
		this->appTableNElem = maxApp;
	}
}


//
// gddAppFuncTable<PV>::read()
//
template <class PV> 
gddAppFuncTableStatus gddAppFuncTable<PV>::read(PV &pv, gdd &value)
{
	gddAppFuncTableStatus status;

	//
	// if this gdd is a container then step through it
	// and fetch all of the values inside
	//
	if (value.isContainer()) {
		gddContainer *pCont = (gddContainer *) &value;
		gddCursor curs = pCont->getCursor();
		gdd *pItem;

		status = S_gddAppFuncTable_Success;
		for (pItem=curs.first(); pItem; pItem=curs.next())
		{
			status = this->read(pv, *pItem);
			if (status) {
				break;
			}
		}
		return status;
	}
	return callReadFunc(pv, value);
}

//
// gddAppFuncTable<PV>::callReadFunc()
//
template <class PV> 
gddAppFuncTableStatus gddAppFuncTable<PV>::callReadFunc (PV &pv, gdd &value)
{
	unsigned type = value.applicationType();
	gddAppReadFunc pFunc;

	//
	// otherwise call the function associated
	// with this application type
	//
	type = value.applicationType();
	if (type>=this->appTableNElem) {
		errPrintf (S_gddAppFuncTable_badType, __FILE__,
			__LINE__, "- large appl type code = %u\n", 
			type);
		return S_gddAppFuncTable_badType;
	}
	pFunc = this->pMFuncRead[type];
	if (pFunc==NULL) {
		errPrintf (S_gddAppFuncTable_badType, __FILE__,
			__LINE__, "- ukn appl type code = %u\n", 
			type);
		return S_gddAppFuncTable_badType;
	}
	return (pv.*pFunc)(value);
}

