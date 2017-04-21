/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
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
#include "gdd.h"
#include "gddAppTable.h"
#include "errMdef.h"
#include "errlog.h"

typedef aitUint32 gddAppFuncTableStatus;

#define S_gddAppFuncTable_Success 0u
#define S_gddAppFuncTable_badType (M_gddFuncTbl|1u) /*application type unregistered*/ 
#define S_gddAppFuncTable_gddLimit (M_gddFuncTbl|2u) /*at gdd lib limit*/ 
#define S_gddAppFuncTable_noMemory (M_gddFuncTbl|3u) /*dynamic memory pool exhausted*/ 

#ifndef NELEMENTS
#define NELEMENTS(array)    (sizeof(array)/sizeof((array)[0]))
#endif

//
// class gddAppFuncTable<PV>
//
template <class PV> 
class gddAppFuncTable {

public:
    gddAppFuncTable() : pMFuncRead(0), appTableNElem(0u) 
    {
    }

    ~gddAppFuncTable() 
    {
        if (this->pMFuncRead) {
            delete [] this->pMFuncRead;
        }
    }

    //
    // installReadFunc()
    //
    // The 2nd parameter should be declared as follows:
    //
    // gddAppFuncTableStatus PV::memberFunction(gdd &value);
    //
    gddAppFuncTableStatus installReadFunc (const unsigned type, 
            gddAppFuncTableStatus (PV::*pMFuncIn)(gdd &) );

    //
    // installReadFunc()
    //
    // The 2nd parameter should be declared as follows:
    //
    // gddAppFuncTableStatus PV::memberFunction(gdd &value);
    //
    gddAppFuncTableStatus installReadFunc ( const char * pName, 
            gddAppFuncTableStatus (PV::*pMFuncIn)(gdd &) );

    //
    //
    //
    gddAppFuncTableStatus read ( PV &pv, gdd &value );
    gddAppFuncTableStatus callReadFunc ( PV &pv, gdd &value );

private:
    //
    // The total number of application tags to manage should be
    // hidden from the application (eventually allow for auto
    // expansion of the table)
    //
    gddAppFuncTableStatus (PV::**pMFuncRead)(gdd &);
    unsigned appTableNElem;

    void newTbl ( unsigned neMaxType );
};

//
// gddAppFuncTable::installReadFunc()
//
// The 2nd parameter should be declared as follows:
//
// gddAppFuncTableStatus PV::memberFunction(gdd &value);
//
// A typedef is not used here because of portability 
// problems resulting from compiler weaknesses
//
template <class PV>
gddAppFuncTableStatus gddAppFuncTable<PV>::installReadFunc (
    const unsigned type, gddAppFuncTableStatus (PV::*pMFuncIn)(gdd &) )
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
// The 2nd parameter should be declared as follows:
//
// gddAppFuncTableStatus PV::memberFunction(gdd &value);
//
template <class PV>
gddAppFuncTableStatus gddAppFuncTable<PV>::installReadFunc (
    const char * pName, gddAppFuncTableStatus (PV::*pMFuncIn)(gdd &) )
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
#   ifdef DEBUG
        printf("installing PV attribute %s = %d\n", pName, type);       
#   endif
    
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
    gddAppFuncTableStatus (PV::**pMNewFuncTbl)(gdd &);
    unsigned maxApp;
    unsigned i;

    if (this->appTableNElem>newApplTypeMax) {
        return;
    }
    maxApp = newApplTypeMax+(1u<<6u);

#   if defined(_MSC_VER) && _MSC_VER <= 1200
        //
        // MS Visual C++ 6.0 (_MSC_VER==1200) or lower 
        // compilers allocate the wrong amount of memory 
        // (i.e. too little) for member function pointers,
        // only explicit calculation via sizeof() works.
        //
        pMNewFuncTbl = (gddAppFuncTableStatus (PV::**)(gdd &))
            new char[sizeof( gddAppFuncTableStatus (PV::*)(gdd &) ) * maxApp];
#   else
	typedef gddAppFuncTableStatus (PV::*pMF_t)(gdd &);
        pMNewFuncTbl = new pMF_t [maxApp];
#   endif
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
    unsigned type;
    gddAppFuncTableStatus (PV::*pFunc)(gdd &);

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
    if ( pFunc == 0 ) {
        errPrintf (S_gddAppFuncTable_badType, __FILE__,
            __LINE__, "- ukn appl type code = %u\n", 
            type);
        return S_gddAppFuncTable_badType;
    }
    return (pv.*pFunc)(value);
}

#endif // gddAppFuncTableH

