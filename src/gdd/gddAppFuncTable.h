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
 * Revision 1.7  1998/02/18 22:52:22  jhill
 * fixed to run on both vis C++ 5.0 and g++
 *
 * Revision 1.6  1998/02/05 22:47:46  jhill
 * workaround vis C++ 5.0 bug
 *
 * Revision 1.5  1997/06/25 06:17:36  jhill
 * fixed warnings
 *
 * Revision 1.4  1996/11/22 20:37:19  jhill
 * converted large inline templ funcs
 *
 * Revision 1.3  1996/09/04 20:58:18  jhill
 * changes for MS VISC++
 *
 * Revision 1.2  1996/08/13 23:13:35  jhill
 * win NT changes
 *
 * Revision 1.1  1996/07/10 23:44:12  jhill
 * moved here from src/cas/generic
 *
 * Revision 1.2  1996/06/26 21:19:01  jhill
 * now matches gdd api revisions
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */

#ifndef gddAppFuncTableH
#define gddAppFuncTableH

//
// ANSI C
//
#include <stdlib.h>

//
// GDD
//
#include <gdd.h>
#include <gddAppTable.h>

typedef aitUint32 gddAppFuncTableStatus;

#define S_gddAppFuncTable_Success 0u
#define S_gddAppFuncTable_badType (M_gddFuncTbl|1u) /*unregisted appl type*/ 
#define S_gddAppFuncTable_gddLimit (M_gddFuncTbl|2u) /*at gdd lib limit*/ 
#define S_gddAppFuncTable_noMemory (M_gddFuncTbl|3u) /*dynamic memory pool exhausted*/ 

#ifndef NELEMENTS
#define NELEMENTS(array)    (sizeof(array)/sizeof((array)[0]))
#endif

//
// template class gddAppFuncTable<PV>
//
template <class PV> 
class gddAppFuncTable {

public:
	gddAppFuncTable() : pMFuncRead(NULL), appTableNElem(0u) 
	{
	}

	~gddAppFuncTable() 
	{
		if (this->pMFuncRead) {
			delete [] this->pMFuncRead;
		}
	}

	//
	// Both cases below are correct C++ syntax 
	//
#if __GNUG__
	//
	// required by gnu g++ 2.7.2
	//
#	define gddAppFuncTablePMF(VAR) gddAppFuncTableStatus (PV:: * VAR)(gdd &)
#	define gddAppFuncTablePMF_t (gddAppFuncTableStatus (PV::*)(gdd &))
#else
	//
	// required by MS vis c++ and also sun pro c++ 
	//
	typedef gddAppFuncTableStatus (PV::*gddAppFuncTablePMF_t)(gdd &);
#	define gddAppFuncTablePMF(VAR) gddAppFuncTablePMF_t VAR
#endif

	//
	// installReadFunc()
	//
	// The 2nd parameter has type "gddAppFuncTablePMF" which is
	// a ptr to member function. The member function should
	// be declared as follows:
	//
	// gddAppFuncTableStatus PV::memberFunction(gdd &value);
	//
	//
	// workaround for bug existing only in microsloth vis c++ 5.0
	//
#if _MSC_VER == 1100
	gddAppFuncTableStatus installReadFuncVISC50 (const unsigned type, 
			gddAppFuncTablePMF(pMFuncIn));
#else
	gddAppFuncTableStatus installReadFunc (const unsigned type, 
			gddAppFuncTablePMF(pMFuncIn));
#endif

	//
	// installReadFunc()
	//
	// The 2nd parameter has type "gddAppFuncTablePMF" which is
	// a ptr to member function. The member function should
	// be declared as follows:
	//
	// gddAppFuncTableStatus PV::memberFunction(gdd &value);
	//
	gddAppFuncTableStatus installReadFunc (const char * pName, 
			gddAppFuncTablePMF(pMFuncIn));

	//
	//
	//
	gddAppFuncTableStatus read(PV &pv, gdd &value);
	gddAppFuncTableStatus callReadFunc (PV &pv, gdd &value);

private:
	//
	// The total number of application tags to manage should be
	// hidden from the application (eventually allow for auto
	// expansion of the table)
	//
	gddAppFuncTablePMF(*pMFuncRead);
	unsigned appTableNElem;

	void newTbl(unsigned neMaxType);
};

//
// gddAppFuncTable::installReadFunc()
//
// The 2nd parameter has type "gddAppFuncTablePMF" which is
// a ptr to member function. The member function should
// be declared as follows:
//
// gddAppFuncTableStatus PV::memberFunction(gdd &value);
//
// The typedef is not used here because of portability 
// problems resulting from compiler weaknesses
//
#if _MSC_VER == 1100
	template <class PV>
	gddAppFuncTableStatus gddAppFuncTable<PV>::installReadFuncVISC50(
		const unsigned type, gddAppFuncTablePMF(pMFuncIn))
#else
	template <class PV>
	gddAppFuncTableStatus gddAppFuncTable<PV>::installReadFunc(
		const unsigned type, gddAppFuncTablePMF(pMFuncIn))
#endif
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
// The 2nd parameter has type "gddAppFuncTablePMF" which is
// a ptr to member function. The member function should
// be declared as follows:
//
// gddAppFuncTableStatus PV::memberFunction(gdd &value);
//
template <class PV>
gddAppFuncTableStatus gddAppFuncTable<PV>::installReadFunc(
	const char * pName, gddAppFuncTablePMF(pMFuncIn))
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
	
#if _MSC_VER == 1100
	return this->installReadFuncVISC50(type, pMFuncIn);
#else
	return this->installReadFunc(type, pMFuncIn);
#endif
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
	gddAppFuncTablePMF(*pMNewFuncTbl);
	unsigned maxApp;
	unsigned i;

	if (this->appTableNElem>newApplTypeMax) {
		return;
	}
	maxApp = newApplTypeMax+(1u<<6u);
#	if defined(_MSC_VER)
		//
		// MS Visual C++ 5.0 (_MSC_VER==1100) or lower 
		// compilers allocate the wrong amount of memory 
		// (i.e. too little) for member function pointers,
		// only explicit calculation via sizeof() works.
		//
		pMNewFuncTbl = (gddAppFuncTablePMF(*))
			new char[sizeof(gddAppFuncTablePMF_t) * maxApp];
#	else
		pMNewFuncTbl = new gddAppFuncTablePMF_t[maxApp];
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
	gddAppFuncTablePMF(pFunc);

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


#endif // gddAppFuncTableH

