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
// 2) This library is not thread safe
//

#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "osiTimer.h"
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
		const bool onceOnly, fdManager &manager, void (*pFunc) (void *pParam), void *pParam);
    epicsShareFunc ~fdRegForOldFdmgr ();

private:
    void (*pFunc) (void *pParam);
    void *pParam;
	epicsShareFunc virtual void callBack ();
};

epicsShareFunc fdRegForOldFdmgr::fdRegForOldFdmgr (const SOCKET fdIn, const fdRegType typeIn, 
	const bool onceOnlyIn, fdManager &managerIn, void (*pFuncIn) (void *pParam), void *pParamIn) :
    fdReg (fdIn, typeIn, onceOnlyIn, managerIn),
    pFunc (pFuncIn),
    pParam (pParamIn)
{
    if (pFuncIn==NULL) {
        throw noFunctionSpecified ();
    }
}

epicsShareFunc fdRegForOldFdmgr::~fdRegForOldFdmgr ()
{
    if (this->pFunc==NULL) {
        throw doubleDelete ();
    }
}

epicsShareFunc void fdRegForOldFdmgr::callBack ()
{
    (*this->pFunc) (this->pParam);
}

class oldFdmgr : public fdManager {
    friend class osiTimerForOldFdmgr;
    friend extern epicsShareFunc int epicsShareAPI fdmgr_clear_timeout (fdctx *pfdctx, fdmgrAlarmId id);
public:

    oldFdmgr ();

private:
    chronIntIdResTable <osiTimerForOldFdmgr> resTbl;
};

//
// osiTimer
//
class osiTimerForOldFdmgr : public osiTimer, public chronIntIdRes<osiTimerForOldFdmgr> {
public:
    //
    // exceptions
    //
    class noFunctionSpecified {};
    class doubleDelete {};

    //
    // create an active timer that will expire in delay seconds
    //
	epicsShareFunc osiTimerForOldFdmgr (oldFdmgr &fdmgr, double delay, void (*pFunc) (void *pParam), void *pParam);
	epicsShareFunc ~osiTimerForOldFdmgr ();

	//
	// called when the osiTimer expires
	//
	epicsShareFunc virtual void expire();

	//
	// for diagnostics
	//
	epicsShareFunc virtual const char *name() const;

private:
    oldFdmgr &fdmgr;
    void (*pFunc) (void *pParam);
    void *pParam;
    unsigned id;
};

osiTimerForOldFdmgr::osiTimerForOldFdmgr (oldFdmgr &fdmgrIn, 
    double delayIn, void (*pFuncIn) (void *pParam), void *pParamIn) :
    fdmgr (fdmgrIn), osiTimer (delayIn, fdmgrIn.timerQueueRef ()), 
    pFunc (pFuncIn), pParam(pParamIn)
{
    if (pFuncIn==NULL) {
        throw noFunctionSpecified ();
    }

    this->fdmgr.resTbl.add (*this);
}

osiTimerForOldFdmgr::~osiTimerForOldFdmgr ()
{
    if (this->pFunc==NULL) {
        throw doubleDelete ();
    }

    this->pFunc = NULL;
    this->pParam = NULL;

    this->fdmgr.resTbl.remove (this->getId());
}

void osiTimerForOldFdmgr::expire ()
{
    (*this->pFunc) (this->pParam);
}

const char * osiTimerForOldFdmgr::name () const
{
    return "osiTimerForOldFdmgr";
}

oldFdmgr::oldFdmgr () : resTbl (1024)
{
}

extern "C" epicsShareFunc fdctx * epicsShareAPI fdmgr_init (void)
{
    oldFdmgr *pfdm;

    try {
        pfdm = new oldFdmgr();
    }
    catch (...)
    {
        return NULL;
    }

    return (fdctx *) pfdm;
}

extern "C" epicsShareFunc fdmgrAlarmId epicsShareAPI fdmgr_add_timeout (
    fdctx *pfdctx, struct timeval *ptimeout, void (*pFunc)(void *pParam), void *pParam)
{
    double delay = ptimeout->tv_sec + ptimeout->tv_usec / static_cast <double> (osiTime::uSecPerSec);
    oldFdmgr *pfdm = static_cast <oldFdmgr *> (pfdctx);
    osiTimerForOldFdmgr *pTimer;
    unsigned id;

    if (!pfdm) {
        return fdmgrNoAlarm;
    }

    try {
        while (true) {
            pTimer = new osiTimerForOldFdmgr (*pfdm, delay, pFunc, pParam);
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
    }
    catch (...)
    {
        id = fdmgrNoAlarm;
    }

    return id;
}

extern "C" epicsShareFunc int epicsShareAPI fdmgr_clear_timeout (fdctx *pfdctx, fdmgrAlarmId id)
{
    oldFdmgr *pfdm = static_cast <oldFdmgr *> (pfdctx);
    osiTimerForOldFdmgr *pTimer;

    try {
        pTimer = pfdm->resTbl.remove (id);
        if (pTimer!=NULL) {
            delete pTimer;
        }
    }
    catch (...)
    {
        return -1;
    }

    return 0;
}

extern "C" epicsShareFunc int epicsShareAPI fdmgr_add_callback (
    fdctx *pfdctx, SOCKET fd, enum fdi_type fdi, void (*pFunc)(void *pParam), void *pParam)
{
    oldFdmgr *pfdm = static_cast <oldFdmgr *> (pfdctx);
    fdRegForOldFdmgr *pfdrbc;
    bool onceOnly = (fdi==fdi_write);

    if (pfdm==NULL) {
        return -1;
    }

    if (pFunc==NULL) {
        return -1;
    }

    if (fdi<0 || fdi>=fdiToFdRegTypeNElements) {
        return -1;
    }

    try {
        pfdrbc = new fdRegForOldFdmgr (fd, fdiToFdRegType[fdi], onceOnly, *pfdm, pFunc, pParam);
    }
    catch (...)
    {
        return -1;
    }

    if (pfdrbc==NULL) {
        return -1;
    }
    return 0;
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
        return -1;
    }

    if (pFDR==NULL) {
        return -1;
    }

    delete pFDR;

    return 0;
}

extern "C" epicsShareFunc int epicsShareAPI fdmgr_pend_event (fdctx *pfdctx, struct timeval *ptimeout)
{
    oldFdmgr *pfdm = static_cast <oldFdmgr *> (pfdctx);
    double delay = ptimeout->tv_sec + ptimeout->tv_usec / static_cast <double> (osiTime::uSecPerSec);

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
