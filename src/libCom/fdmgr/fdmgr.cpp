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
		const bool onceOnly, fdManager &manager, pCallBackFDMgr pFunc, void *pParam);
    epicsShareFunc ~fdRegForOldFdmgr ();

private:
    pCallBackFDMgr pFunc;
    void *pParam;
	epicsShareFunc virtual void callBack ();
};

class oldFdmgr;

//
// osiTimerForOldFdmgr
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
	epicsShareFunc osiTimerForOldFdmgr (oldFdmgr &fdmgr, double delay, pCallBackFDMgr pFunc, void *pParam);
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
    pCallBackFDMgr pFunc;
    void *pParam;
    unsigned id;
};

class oldFdmgr : public fdManager {
    friend class osiTimerForOldFdmgr;
    friend epicsShareFunc int epicsShareAPI fdmgr_clear_timeout (fdctx *pfdctx, fdmgrAlarmId id);

public:
    oldFdmgr ();

private:
    chronIntIdResTable <osiTimerForOldFdmgr> resTbl;
};

epicsShareFunc fdRegForOldFdmgr::fdRegForOldFdmgr (const SOCKET fdIn, const fdRegType typeIn, 
	const bool onceOnlyIn, fdManager &managerIn, pCallBackFDMgr pFuncIn, void *pParamIn) :
    fdReg (fdIn, typeIn, onceOnlyIn, managerIn),
    pFunc (pFuncIn),
    pParam (pParamIn)
{
    if (pFuncIn==NULL) {
#       ifdef noExceptionsFromCXX
            assert (0);
#       else            
            throw noFunctionSpecified ();
#       endif
    }
}

epicsShareFunc fdRegForOldFdmgr::~fdRegForOldFdmgr ()
{
    if (this->pFunc==NULL) {
#       ifdef noExceptionsFromCXX
            assert (0);
#       else            
            throw doubleDelete ();
#       endif
    }
}

epicsShareFunc void fdRegForOldFdmgr::callBack ()
{
    (*this->pFunc) (this->pParam);
}

osiTimerForOldFdmgr::osiTimerForOldFdmgr (oldFdmgr &fdmgrIn, 
    double delayIn, pCallBackFDMgr pFuncIn, void *pParamIn) :
    osiTimer (delayIn, fdmgrIn.timerQueueRef ()), 
    fdmgr (fdmgrIn), pFunc (pFuncIn), pParam(pParamIn)
{
    if (pFuncIn==NULL) {
#       ifdef noExceptionsFromCXX
            assert (0);
#       else            
            throw noFunctionSpecified ();
#       endif
    }

    this->fdmgr.resTbl.add (*this);
}

osiTimerForOldFdmgr::~osiTimerForOldFdmgr ()
{
    if (this->pFunc==NULL) {
#       ifdef noExceptionsFromCXX
            assert (0);
#       else            
            throw doubleDelete ();
#       endif
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

#   ifdef noExceptionsFromCXX
        pfdm = new oldFdmgr();
#   else            
        try {
            pfdm = new oldFdmgr();
        }
        catch (...)
        {
            pfdm = NULL;
        }
#   endif

    return (fdctx *) pfdm;
}

extern "C" epicsShareFunc fdmgrAlarmId epicsShareAPI fdmgr_add_timeout (
    fdctx *pfdctx, struct timeval *ptimeout, pCallBackFDMgr pFunc, void *pParam)
{
    double delay = ptimeout->tv_sec + ptimeout->tv_usec / static_cast <double> (osiTime::uSecPerSec);
    oldFdmgr *pfdm = static_cast <oldFdmgr *> (pfdctx);
    osiTimerForOldFdmgr *pTimer;
    unsigned id;

    if (!pfdm) {
        return fdmgrNoAlarm;
    }

    while (true) {
#       ifdef noExceptionsFromCXX
            pTimer = new osiTimerForOldFdmgr (*pfdm, delay, pFunc, pParam);
#       else            
            try {
                pTimer = new osiTimerForOldFdmgr 
			(*pfdm, delay, pFunc, pParam);
            }
            catch (...)
            {
                pTimer = NULL;
            }
#       endif       
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
    osiTimerForOldFdmgr *pTimer;

#   ifdef noExceptionsFromCXX
        pTimer = pfdm->resTbl.remove (id);
#   else
        try {
            pTimer = pfdm->resTbl.remove (id);
        }
        catch (...)
        {
            pTimer = NULL;
        }
#   endif

    if (pTimer==NULL) {
        return -1;
    }
    else {
        delete pTimer;
        return 0;
    }
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

#   ifdef noExceptionsFromCXX
        pfdrbc = new fdRegForOldFdmgr (fd, fdiToFdRegType[fdiType], onceOnly, *pfdm, pFunc, pParam);
#   else
        try {
            pfdrbc = new fdRegForOldFdmgr (fd, fdiToFdRegType[fdiType], onceOnly, *pfdm, pFunc, pParam);
        }
        catch (...)
        {
            pfdrbc = NULL;
        }
#   endif

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

#   ifdef noExceptionsFromCXX
        pFDR = pfdm->lookUpFD (fd, fdiToFdRegType[fdi]);
#   else
        try {
            pFDR = pfdm->lookUpFD (fd, fdiToFdRegType[fdi]);
        }
        catch (...)
        {
            pFDR = NULL;
        }
#   endif

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
    double delay = ptimeout->tv_sec + ptimeout->tv_usec / static_cast <double> (osiTime::uSecPerSec);

#   ifdef noExceptionsFromCXX
        pfdm->process (delay);
#   else
        try {
            pfdm->process (delay);
        }
        catch (...) {
            return -1;
        }
#   endif

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
