/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
//      File descriptor management C++ class library
//      (for multiplexing IO in a single threaded environment)
//
//      Author  Jeffrey O. Hill
//              johill@lanl.gov
//              505 665 1831
//
// NOTES: 
// 1) the routines in this file provide backward compatibility with the original
// "C" based file descriptor manager API
// 2) This library is _not_ thread safe
//

#include <stddef.h>
#define epicsExportSharedSymbols
#include "locationException.h"
#include "epicsAssert.h"
#include "fdManager.h"
#include "fdmgr.h"
 
static const fdRegType fdiToFdRegType[] = {fdrRead, fdrWrite, fdrException};
static const unsigned fdiToFdRegTypeNElements = sizeof (fdiToFdRegType) / sizeof (fdiToFdRegType[0]);
const unsigned mSecPerSec = 1000u;
const unsigned uSecPerSec = 1000u * mSecPerSec;

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
	fdRegForOldFdmgr ( const fdRegForOldFdmgr & );
	fdRegForOldFdmgr & operator = ( const fdRegForOldFdmgr & );
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
	timerForOldFdmgr ( const timerForOldFdmgr & );
	timerForOldFdmgr & operator = ( const timerForOldFdmgr & );
};

class oldFdmgr : public fdManager {
    friend class timerForOldFdmgr;
    friend epicsShareFunc int epicsShareAPI fdmgr_clear_timeout (fdctx *pfdctx, fdmgrAlarmId id);

public:
    oldFdmgr ();

private:
    chronIntIdResTable <timerForOldFdmgr> resTbl;
	oldFdmgr ( const oldFdmgr & );
	oldFdmgr & operator = ( const oldFdmgr & );
};

#ifdef _MSC_VER
#   pragma warning ( push )
#   pragma warning ( disable:4660 )
#endif

template class chronIntIdResTable <timerForOldFdmgr>;
template class resTable<timerForOldFdmgr, chronIntId>;

#ifdef _MSC_VER
#   pragma warning ( pop )
#endif

epicsShareFunc fdRegForOldFdmgr::fdRegForOldFdmgr 
    (const SOCKET fdIn, const fdRegType typeIn, 
	    const bool onceOnlyIn, fdManager &managerIn, 
        pCallBackFDMgr pFuncIn, void *pParamIn) :
    fdReg (fdIn, typeIn, onceOnlyIn, managerIn), 
        pFunc (pFuncIn), pParam (pParamIn)
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
    this->fdmgr.resTbl.idAssignAdd (*this);
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
    double delay = ptimeout->tv_sec + ptimeout->tv_usec / static_cast <const double> (uSecPerSec);
    oldFdmgr *pfdm = static_cast <oldFdmgr *> (pfdctx);
    timerForOldFdmgr *pTimer;
    unsigned id = fdmgrNoAlarm;

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
    double delay = ptimeout->tv_sec + ptimeout->tv_usec / static_cast <const double> (uSecPerSec);

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
