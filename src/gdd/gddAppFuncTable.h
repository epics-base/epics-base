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

template <class PV> 
class gddAppFuncTable {

public:
	gddAppFuncTable(); 

	//
	// typedef for the app read function to be called
	//
	typedef gddAppFuncTableStatus (PV::*gddAppReadFunc)(gdd &value);

	//
	// installReadFunc()
	// (g++ gags when these are coded outside the class def?) 
	//
	gddAppFuncTableStatus installReadFunc(const unsigned type, gddAppReadFunc pMFuncIn)
	{
		//
		// Attempt to expand the table if the app type will not fit 
		//
		if (type>=this->maxAppType) {
			this->newTbl(type);
			if (type>=this->maxAppType) {
				return S_gddAppFuncTable_noMemory;
			}
		}
		this->pMFuncRead[type]=pMFuncIn;
		return S_gddAppFuncTable_Success;
	}
	gddAppFuncTableStatus installReadFunc(const char * const pName, gddAppReadFunc pMFuncIn)
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
#		ifdef DEBUG
			printf("installing PV attribute %s = %d\n", pName, type);		
#		endif
		return this->installReadFunc(type, pMFuncIn);
	}

	//
	//
	//
	gddAppFuncTableStatus read(PV &pv, gdd &value);

private:
	//
	// The total number of application tags to manage should be
	// hidden from the application (eventually allow for auto
	// expansion of the table)
	//
	gddAppReadFunc *pMFuncRead;
	unsigned maxAppType;

	void newTbl(unsigned neMaxType);
};

//
// gddAppFuncTable<PV>::gddAppFuncTable() 
// 
// The total number of application tags to manage should be
// hidden from the application 
//
template <class PV> 
inline gddAppFuncTable<PV>::gddAppFuncTable() : 
	pMFuncRead(NULL),
	maxAppType(0u)
{
}

//
// gddAppFuncTable<PV>::newTbl() 
// 
// The total number of application tags to manage should be
// hidden from the application 
//
template <class PV> 
inline void gddAppFuncTable<PV>::newTbl(unsigned newApplTypeMax) 
{
	gddAppReadFunc *pMNewFuncTbl;
	unsigned maxApp;

	if(this->maxAppType>=newApplTypeMax) {
		return;
	}
	maxApp = newApplTypeMax+(1u<<6u);
	pMNewFuncTbl = new gddAppReadFunc[maxApp];
	if (pMNewFuncTbl) {
		if (this->pMFuncRead) {
			memcpy(	pMNewFuncTbl, 
				this->pMFuncRead,
				this->maxAppType*sizeof(*pMNewFuncTbl));
			delete [] this->pMFuncRead;
			memset(&pMNewFuncTbl[this->maxAppType], 0, 
				(maxApp-this->maxAppType) * 
					sizeof(*pMNewFuncTbl));
		}
		else {
			memset(pMNewFuncTbl, 0, 
				maxApp * sizeof(*pMNewFuncTbl));
		}
		this->pMFuncRead = pMNewFuncTbl;
		this->maxAppType = maxApp;
	}
}

//
// gddAppFuncTable<PV>::installReadFunc()
//


//
// gddAppFuncTable<PV>::read()
//
// (g++ generates "multiply defined symbols" message unless I set this 
// to be inline)
//
template <class PV> 
inline gddAppFuncTableStatus gddAppFuncTable<PV>::read(PV &pv, gdd &value)
{
	gddAppFuncTableStatus status;
	gddAppReadFunc pFunc;
	unsigned type;

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

	//
	// otherwise call the function associated
	// with this application type
	//
	type = value.applicationType();
	if (type>=this->maxAppType) {
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
	status = (pv.*pFunc)(value);
	return status;
}

