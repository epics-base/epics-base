//
//      $Id$
//
//      File descriptor management C++ class library
//      (for multiplexing IO in a single threaded environment)
//
//      Author  Jeffrey O. Hill
//              johill@lanl.gov
//              505 665 1831
//
//      Experimental Physics and Industrial Control System (EPICS)
//
//      Copyright 1991, the Regents of the University of California,
//      and the University of Chicago Board of Governors.
//
//      This software was produced under  U.S. Government contracts:
//      (W-7405-ENG-36) at the Los Alamos National Laboratory,
//      and (W-31-109-ENG-38) at Argonne National Laboratory.
//
//      Initial development by:
//              The Controls and Automation Group (AT-8)
//              Ground Test Accelerator
//              Accelerator Technology Division
//              Los Alamos National Laboratory
//
//      Co-developed with
//              The Controls and Computing Group
//              Accelerator Systems Division
//              Advanced Photon Source
//              Argonne National Laboratory
//
// NOTES: 
// 1) the routines in this file provide backward compatibility with the original
// "C" based file descriptor manager API
// 2) This library is _not_ thread safe
//

#include <stddef.h>
#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "fdManager.h"
#include "fdmgr.h"
 
static const fdRegType fdiToFdRegType[] = {fdrRead, fdrWrite, fdrException};
static const unsigned fdiToFdRegTypeNElements = sizeof (fdiToFdRegType) / sizeof (fdiToFdRegType[0]);

class fdRegForOldFdmgr : public fdReg {
public:
    //
    // exceptions
    //
    class noFunctionSpecified {};
    class doubleDelete {};

    epicsShareFunc fdRegForOldFdmgr (const SOCKET fdIn, const fdRegType type, 
		const bool onceOnly, fdManager &manager, pCallBackFDMgr pFunc, void *pParam);
    epicsShareFunc ~fdRegForOldFdmgr ();

private:
    pCallBackFDMgr pFunc;
    void *pParam;
	epicsShareFunc virtual void callBack ();
};

class oldFdmgr;

//
// timerForOldFdmgr
//
class timerForOldFdmgr : public epicsTimerNotify, public chronIntIdRes<timerForOldFdmgr> {
public:
	epicsShareFunc timerForOldFdmgr (oldFdmgr &fdmgr, double delay, pCallBackFDMgr pFunc, void *pParam);
	epicsShareFunc virtual ~timerForOldFdmgr ();

    //
    // exceptions
    //
    class noFunctionSpecified {};
    class doubleDelete {};
private:
    epicsTimer &timer;
    oldFdmgr &fdmgr;
    pCallBackFDMgr pFunc;
    void *pParam;
    unsigned id;
    epicsShareFunc expireStatus expire ( const epicsTime & currentTime );
};

class oldFdmgr : public fdManager {
    friend class timerForOldFdmgr;
    friend epicsShareFunc int epicsShareAPI fdmgr_clear_timeout (fdctx *pfdctx, fdmgrAlarmId id);

public:
    oldFdmgr ();

private:
    chronIntIdResTable <timerForOldFdmgr> resTbl;
};

epicsShareFunc fdRegForOldFdmgr::fdRegForOldFdmgr (const SOCKET fdIn, const fdRegType typeIn, 
	const bool onceOnlyIn, fdManager &managerIn, pCallBackFDMgr pFuncIn, void *pParamIn) :
    fdReg (fdIn, typeIn, onceOnlyIn),
    pFunc (pFuncIn),
    pParam (pParamIn)
{
    if (pFuncIn==NULL) {
        throwWithLocation ( noFunctionSpecified () );
    }
}

epicsShareFunc fdRegForOldFdmgr::~fdRegForOldFdmgr ()
{
    if (this->pFunc==NULL) {
        throwWithLocation ( doubleDelete () );
    }
}

epicsShareFunc void fdRegForOldFdmgr::callBack ()
{
    (*this->pFunc) (this->pParam);
}

timerForOldFdmgr::timerForOldFdmgr ( oldFdmgr &fdmgrIn, 
    double delayIn, pCallBackFDMgr pFuncIn, void * pParamIn ) :
    timer ( fdmgrIn.createTimer() ), 
    fdmgr ( fdmgrIn ), pFunc ( pFuncIn ), pParam( pParamIn )
{
    if ( pFuncIn == NULL ) {
        throwWithLocation ( noFunctionSpecified () );
    }
    this->fdmgr.resTbl.add (*this);
    this->timer.start ( *this, delayIn );
}

timerForOldFdmgr::~timerForOldFdmgr ()
{
    this->fdmgr.resTbl.remove ( this->getId() );
    this->timer.destroy ();
}

epicsTimerNotify::expireStatus timerForOldFdmgr::expire ( const epicsTime & )
{
    (*this->pFunc) (this->pParam);
    return noRestart;
}

oldFdmgr::oldFdmgr () {}

extern "C" epicsShareFunc fdctx * epicsShareAPI fdmgr_init (void)
{
    oldFdmgr *pfdm;

    try {
        pfdm = new oldFdmgr();
    }
    catch (...)
    {
        pfdm = NULL;
    }

    return (fdctx *) pfdm;
}

extern "C" epicsShareFunc fdmgrAlarmId epicsShareAPI fdmgr_add_timeout (
    fdctx *pfdctx, struct timeval *ptimeout, pCallBackFDMgr pFunc, void *pParam)
{
    double delay = ptimeout->tv_sec + ptimeout->tv_usec / static_cast <const double> (epicsTime::uSecPerSec);
    oldFdmgr *pfdm = static_cast <oldFdmgr *> (pfdctx);
    timerForOldFdmgr *pTimer;
    unsigned id;

    if (!pfdm) {
        return fdmgrNoAlarm;
    }

    while (true) {
        try {
            pTimer = new timerForOldFdmgr 
			    (*pfdm, delay, pFunc, pParam);
        }
        catch (...)
        {
            pTimer = NULL;
        }
        if (pTimer) {
            id = pTimer->getId ();
            if (id!=fdmgrNoAlarm) {
                break;
            }
            else {
                delete pTimer;
            }
        }
        else {
            id = fdmgrNoAlarm;
            break;
        }
    }

    return id;
}

extern "C" epicsShareFunc int epicsShareAPI fdmgr_clear_timeout (fdctx *pfdctx, fdmgrAlarmId id)
{
    oldFdmgr *pfdm = static_cast <oldFdmgr *> (pfdctx);
    timerForOldFdmgr *pTimer;

    try {
        pTimer = pfdm->resTbl.remove (id);
    }
    catch (...)
    {
        pTimer = NULL;
    }

    if (pTimer==NULL) {
        return -1;
    }
    delete pTimer;
    return 0;
}

extern "C" epicsShareFunc int epicsShareAPI fdmgr_add_callback (
    fdctx *pfdctx, SOCKET fd, enum fdi_type fdi, pCallBackFDMgr pFunc, void *pParam)
{
    oldFdmgr *pfdm = static_cast <oldFdmgr *> (pfdctx);
    fdRegForOldFdmgr *pfdrbc;
    bool onceOnly = (fdi==fdi_write);
    unsigned fdiType;

    if (pfdm==NULL) {
        return -1;
    }

    if (pFunc==NULL) {
        return -1;
    }

    if (fdi<0) {
        return -1;
    }

    fdiType = (unsigned) fdi;

    if (fdiType>=fdiToFdRegTypeNElements) {
        return -1;
    }

    try {
        pfdrbc = new fdRegForOldFdmgr (fd, fdiToFdRegType[fdiType], onceOnly, *pfdm, pFunc, pParam);
    }
    catch (...)
    {
        pfdrbc = NULL;
    }

    if (pfdrbc==NULL) {
        return -1;
    }
    else {
        return 0;
    }
}
 
extern "C" epicsShareFunc int epicsShareAPI fdmgr_clear_callback (
    fdctx *pfdctx, SOCKET fd, enum fdi_type	fdi)
{
    oldFdmgr *pfdm = static_cast <oldFdmgr *> (pfdctx);
    fdReg *pFDR;

    if (pfdm==NULL) {
        return -1;
    }

    try {
        pFDR = pfdm->lookUpFD (fd, fdiToFdRegType[fdi]);
    }
    catch (...)
    {
        pFDR = NULL;
    }

    if (pFDR==NULL) {
        return -1;
    }
    else {
        delete pFDR;
        return 0;
    }
}

extern "C" epicsShareFunc int epicsShareAPI fdmgr_pend_event (fdctx *pfdctx, struct timeval *ptimeout)
{
    oldFdmgr *pfdm = static_cast <oldFdmgr *> (pfdctx);
    double delay = ptimeout->tv_sec + ptimeout->tv_usec / static_cast <const double> (epicsTime::uSecPerSec);

    try {
        pfdm->process (delay);
    }
    catch (...) {
        return -1;
    }

    return 0;
}

extern "C" epicsShareFunc int epicsShareAPI fdmgr_delete (fdctx *pfdctx)
{
    oldFdmgr *pfdm = static_cast <oldFdmgr *> (pfdctx);
    delete pfdm;
    return 0;
}

/*
 * depricated interface
 */
extern "C" epicsShareFunc int epicsShareAPI fdmgr_clear_fd (fdctx *pfdctx, SOCKET fd)
{
	return fdmgr_clear_callback(pfdctx, fd, fdi_read);
}

/*
 * depricated interface
 */
extern "C" epicsShareFunc int epicsShareAPI fdmgr_add_fd ( 
    fdctx   *pfdctx, SOCKET  fd, void (*pfunc)(void *pParam), void *param)
{
	return fdmgr_add_callback (pfdctx, fd, fdi_read, pfunc, param);
}
