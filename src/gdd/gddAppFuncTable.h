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
	// typedef for the app read function to be called
	//
	typedef gddAppFuncTableStatus (PV::*gddAppReadFunc)(gdd &value);

	//
	// installReadFunc()
        //
        // The 2nd parameter has type "gddAppReadFunc" which is
        // a ptr to member function. The member function should
        // be declared as follows:
        //
        // gddAppFuncTableStatus PV::memberFunction(gdd &value);
        //
	gddAppFuncTableStatus installReadFunc(const unsigned type, 
			gddAppReadFunc pMFuncIn);

	//
	// installReadFunc()
        //
        // The 2nd parameter has type "gddAppReadFunc" which is
        // a ptr to member function. The member function should
        // be declared as follows:
        //
        // gddAppFuncTableStatus PV::memberFunction(gdd &value);
        //
	gddAppFuncTableStatus installReadFunc(const char * const pName, 
			gddAppReadFunc pMFuncIn);

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
	gddAppReadFunc *pMFuncRead;
	unsigned appTableNElem;

	void newTbl(unsigned neMaxType);
};

#endif // gddAppFuncTableH

