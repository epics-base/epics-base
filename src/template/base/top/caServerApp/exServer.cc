/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
//
// fileDescriptorManager.process(delay);
// (the name of the global symbol has leaked in here)
//

//
// Example EPICS CA server
//
#include "exServer.h"

//
// static list of pre-created PVs
//
pvInfo exServer::pvList[] = {
    pvInfo (1.0e-1, "jane", 10.0f, 0.0f, aitEnumFloat64, excasIoSync, 1u),
    pvInfo (2.0, "fred", 10.0f, -10.0f, aitEnumFloat64, excasIoSync, 1u),
    pvInfo (1.0e-1, "janet", 10.0f, 0.0f, aitEnumFloat64, excasIoAsync, 1u),
    pvInfo (2.0, "freddy", 10.0f, -10.0f, aitEnumFloat64, excasIoAsync, 1u),
    pvInfo (2.0, "alan", 10.0f, -10.0f, aitEnumFloat64, excasIoSync, 100u),
    pvInfo (20.0, "albert", 10.0f, -10.0f, aitEnumFloat64, excasIoSync, 1000u),
    pvInfo (-1.0, "boot", 10.0f, -10.0f, aitEnumEnum16, excasIoSync, 1u),
    pvInfo (1.0, "booty", 10.0f, -10.0f, aitEnumEnum16, excasIoAsync, 1u),
    pvInfo (-1.0, "bill", 10.0f, -10.0f, aitEnumFloat64, excasIoSync, 1u),
    pvInfo (-1.0, "billy", 10.0f, -10.0f, aitEnumFloat64, excasIoAsync, 1u)
};

const unsigned exServer::pvListNElem = NELEMENTS (exServer::pvList);

//
// static on-the-fly PVs
//
pvInfo exServer::billy (-1.0, "billybob", 10.0f, -10.0f, aitEnumFloat64, excasIoAsync, 1u);
pvInfo exServer::bloater (.010, "bloater", 10.0f, -10.0f, aitEnumFloat64, excasIoSync, 10000u);
pvInfo exServer::bloaty (.010, "bloaty", 10.0f, -10.0f, aitEnumFloat64, excasIoSync, 100000u);


//
// exServer::exServer()
//
exServer::exServer ( const char * const pvPrefix, 
        unsigned aliasCount, bool scanOnIn, 
        bool asyncScan, double asyncDelayIn,
        unsigned maxSimultAsyncIOIn ) : 
        pTimerQueue ( 0 ), simultAsychIOCount ( 0u ), 
        _maxSimultAsyncIO ( maxSimultAsyncIOIn ),
        asyncDelay ( asyncDelayIn ), scanOn ( scanOnIn )
{
    unsigned i;
    exPV *pPV;
    pvInfo *pPVI;
    pvInfo *pPVAfter = &exServer::pvList[pvListNElem];
    char pvAlias[256];
    const char * const pNameFmtStr = "%.100s%.20s";
    const char * const pAliasFmtStr = "%.100s%.20s%.6u";

    exPV::initFT();

    if ( asyncScan ) {
        unsigned timerPriotity;
        epicsThreadBooleanStatus etbs = epicsThreadLowestPriorityLevelAbove (
                epicsThreadGetPrioritySelf (), & timerPriotity );
        if ( etbs != epicsThreadBooleanStatusSuccess ) {
            timerPriotity = epicsThreadGetPrioritySelf ();
        }
        this->pTimerQueue = & epicsTimerQueueActive::allocate ( false, timerPriotity );
    }

    //
    // pre-create all of the simple PVs that this server will export
    //
    for (pPVI = exServer::pvList; pPVI < pPVAfter; pPVI++) {
        pPV = pPVI->createPV (*this, true, scanOnIn,  this->asyncDelay );
        if (!pPV) {
            fprintf(stderr, "Unable to create new PV \"%s\"\n",
                pPVI->getName());
        }


        //
        // Install canonical (root) name
        //
        sprintf(pvAlias, pNameFmtStr, pvPrefix, pPVI->getName());
        this->installAliasName(*pPVI, pvAlias);

        //
        // Install numbered alias names
        //
        for (i=0u; i<aliasCount; i++) {
            sprintf(pvAlias, pAliasFmtStr, pvPrefix,  
                    pPVI->getName(), i);
            this->installAliasName(*pPVI, pvAlias);
        }
    }

    //
    // Install create on-the-fly PVs 
    // into the PV name hash table
    //
    sprintf ( pvAlias, pNameFmtStr, pvPrefix, billy.getName() );
    this->installAliasName ( billy, pvAlias );
    sprintf ( pvAlias, pNameFmtStr, pvPrefix, bloater.getName() );
    this->installAliasName ( bloater, pvAlias );
    sprintf ( pvAlias, pNameFmtStr, pvPrefix, bloaty.getName() );
    this->installAliasName ( bloaty, pvAlias );
}

//
// exServer::~exServer()
//
exServer::~exServer()
{
    this->destroyAllPV ();   
    this->stringResTbl.traverse ( &pvEntry::destroy );
}

void exServer::destroyAllPV ()
{
    for ( unsigned i = 0; 
            i < NELEMENTS(exServer::pvList); i++ ) {
        exServer::pvList[i].deletePV ();
    }
}

//
// exServer::installAliasName()
//
void exServer::installAliasName(pvInfo &info, const char *pAliasName)
{
    pvEntry *pEntry;

    pEntry = new pvEntry(info, *this, pAliasName);
    if (pEntry) {
        int resLibStatus;
        resLibStatus = this->stringResTbl.add(*pEntry);
        if (resLibStatus==0) {
            return;
        }
        else {
            delete pEntry;
        }
    }
    fprintf ( stderr, 
"Unable to enter PV=\"%s\" Alias=\"%s\" in PV name alias hash table\n",
        info.getName(), pAliasName );
}

//
// More advanced pvExistTest() isnt needed so we forward to
// original version. This avoids sun pro warnings and speeds 
// up execution.
//
pvExistReturn exServer::pvExistTest
	( const casCtx & ctx, const caNetAddr &, const char * pPVName )
{
	return this->pvExistTest ( ctx, pPVName );
}

//
// exServer::pvExistTest()
//
pvExistReturn exServer::pvExistTest
    ( const casCtx& ctxIn, const char * pPVName )
{
    //
    // lifetime of id is shorter than lifetime of pName
    //
    stringId id ( pPVName, stringId::refString );
    pvEntry *pPVE;

    //
    // Look in hash table for PV name (or PV alias name)
    //
    pPVE = this->stringResTbl.lookup ( id );
    if ( ! pPVE ) {
        return pverDoesNotExistHere;
    }

    pvInfo & pvi = pPVE->getInfo();

    //
    // Initiate async IO if this is an async PV
    //
    if ( pvi.getIOType() == excasIoSync ) {
        return pverExistsHere;
    }
    else {
        if ( this->simultAsychIOCount >= this->_maxSimultAsyncIO ) {
            return pverDoesNotExistHere;
        }

        this->simultAsychIOCount++;

        exAsyncExistIO * pIO = 
            new exAsyncExistIO ( pvi, ctxIn, *this );
        if ( pIO ) {
            return pverAsyncCompletion;
        }
        else {
            this->simultAsychIOCount--;
            return pverDoesNotExistHere;
        }
    }
}

//
// exServer::pvAttach()
//
pvAttachReturn exServer::pvAttach
    (const casCtx &ctx, const char *pName)
{
    //
    // lifetime of id is shorter than lifetime of pName
    //
    stringId id(pName, stringId::refString); 
    exPV *pPV;
    pvEntry *pPVE;

    pPVE = this->stringResTbl.lookup(id);
    if (!pPVE) {
        return S_casApp_pvNotFound;
    }

    pvInfo &pvi = pPVE->getInfo();

    //
    // If this is a synchronous PV create the PV now 
    //
    if (pvi.getIOType() == excasIoSync) {
        pPV = pvi.createPV(*this, false, this->scanOn, this->asyncDelay );
        if (pPV) {
            return *pPV;
        }
        else {
            return S_casApp_noMemory;
        }
    }
    //
    // Initiate async IO if this is an async PV
    //
    else {
        if (this->simultAsychIOCount>=this->_maxSimultAsyncIO) {
            return S_casApp_postponeAsyncIO;
        }

        this->simultAsychIOCount++;

        exAsyncCreateIO *pIO = 
            new exAsyncCreateIO ( pvi, *this, ctx, 
                this->scanOn, this->asyncDelay );
        if (pIO) {
            return S_casApp_asyncCompletion;
        }
        else {
            this->simultAsychIOCount--;
            return S_casApp_noMemory;
        }
    }
}

//
// exServer::setDebugLevel ()
//
void exServer::setDebugLevel ( unsigned level )
{
    this->caServer::setDebugLevel ( level );
}

//
// exServer::createTimer ()
//
class epicsTimer & exServer::createTimer ()
{
    if ( this->pTimerQueue ) {
        return this->pTimerQueue->createTimer ();
    }
    else {
        return this->caServer::createTimer ();
    }
}

//
// pvInfo::createPV()
//
exPV *pvInfo::createPV ( exServer & cas, bool preCreateFlag, 
                    bool scanOn, double asyncDelay )
{
    if (this->pPV) {
        return this->pPV;
    }

    exPV *pNewPV;

    //
    // create an instance of the appropriate class
    // depending on the io type and the number
    // of elements
    //
    if (this->capacity==1u) {
        switch (this->ioType){
        case excasIoSync:
            pNewPV = new exScalarPV ( cas, *this, preCreateFlag, scanOn );
            break;
        case excasIoAsync:
            pNewPV = new exAsyncPV ( cas, *this, 
                preCreateFlag, scanOn, asyncDelay );
            break;
        default:
            pNewPV = NULL;
            break;
        }
    }
    else {
        if ( this->ioType == excasIoSync ) {
            pNewPV = new exVectorPV ( cas, *this, preCreateFlag, scanOn );
        }
        else {
            pNewPV = NULL;
        }
    }
    
    //
    // load initial value (this is not done in
    // the constructor because the base class's
    // pure virtual function would be called)
    //
    // We always perform this step even if
    // scanning is disable so that there will
    // always be an initial value
    //
    if (pNewPV) {
        this->pPV = pNewPV;
        pNewPV->scan();
    }

    return pNewPV;
}

//
// exServer::show() 
//
void exServer::show (unsigned level) const
{
    //
    // server tool specific show code goes here
    //
    this->stringResTbl.show(level);

    //
    // print information about ca server libarary
    // internals
    //
    this->caServer::show(level);
}

//
// exAsyncExistIO::exAsyncExistIO()
//
exAsyncExistIO::exAsyncExistIO ( const pvInfo &pviIn, const casCtx &ctxIn,
        exServer &casIn ) :
    casAsyncPVExistIO ( ctxIn ), pvi ( pviIn ), 
        timer ( casIn.createTimer () ), cas ( casIn ) 
{
    this->timer.start ( *this, 0.00001 );
}

//
// exAsyncExistIO::~exAsyncExistIO()
//
exAsyncExistIO::~exAsyncExistIO()
{
    this->cas.removeIO ();
    this->timer.destroy ();
}

//
// exAsyncExistIO::expire()
// (a virtual function that runs when the base timer expires)
//
epicsTimerNotify::expireStatus exAsyncExistIO::expire ( const epicsTime & /*currentTime*/ )
{
    //
    // post IO completion
    //
    this->postIOCompletion ( pvExistReturn(pverExistsHere) );
    return noRestart;
}


//
// exAsyncCreateIO::exAsyncCreateIO()
//
exAsyncCreateIO :: 
    exAsyncCreateIO ( pvInfo &pviIn, exServer &casIn, 
    const casCtx &ctxIn, bool scanOnIn, double asyncDelayIn ) :
    casAsyncPVAttachIO ( ctxIn ), pvi ( pviIn ), 
        timer ( casIn.createTimer () ), 
        cas ( casIn ), asyncDelay ( asyncDelayIn ), scanOn ( scanOnIn )
{
    this->timer.start ( *this, 0.00001 );
}

//
// exAsyncCreateIO::~exAsyncCreateIO()
//
exAsyncCreateIO::~exAsyncCreateIO()
{
    this->cas.removeIO ();
    this->timer.destroy ();
}

//
// exAsyncCreateIO::expire()
// (a virtual function that runs when the base timer expires)
//
epicsTimerNotify::expireStatus exAsyncCreateIO::expire ( const epicsTime & /*currentTime*/ )
{
    exPV * pPV = this->pvi.createPV ( this->cas, false, 
                            this->scanOn, this->asyncDelay );
    if ( pPV ) {
        this->postIOCompletion ( pvAttachReturn ( *pPV ) );
    }
    else {
        this->postIOCompletion ( pvAttachReturn ( S_casApp_noMemory ) );
    }
    return noRestart;
}

