/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* 
 * Archictecture dependent support for common device driver resources 
 *
 *      Author: Jeff Hill 
 *      Date: 10-30-98  
 */

#include <stdlib.h>

#include <vxWorks.h>
#include <types.h>
#include <iv.h>
#include <vme.h>
#include <sysLib.h>
#include <memLib.h>
#include <intLib.h>
#include <logLib.h>
#include <vxLib.h>

#include "epicsFindSymbol.h"
#include "devLibVME.h"
#include "errlog.h"

typedef void    myISR (void *pParam);

#if CPU_FAMILY != PPC
/*
 * A list of the names of the unexpected interrupt handlers
 * ( some of these are provided by wrs )
 */
static char  *defaultHandlerNames[] = {
            "excStub",
            "excIntStub",
            "unsolicitedHandlerEPICS"};

static myISR *defaultHandlerAddr[NELEMENTS(defaultHandlerNames)];
#endif

static myISR *isrFetch(unsigned vectorNumber);

/*
 * this routine needs to be in the symbol table
 * (i.e. not static) for this code to work correctly
 */
void unsolicitedHandlerEPICS(int vectorNumber);

/*
 * this is in veclist.c
 */
int cISRTest(void (*)(), void (**)(), void **);

/*
 * Make sure that the CR/CSR addressing mode is defined.
 * (it may not be in older versions of vxWorks)
 */
#ifndef VME_AM_CSR
#  define VME_AM_CSR (0x2f)
#endif

/*
 * we use a translation between an EPICS encoding
 * and a vxWorks encoding here
 * to reduce dependency of drivers on vxWorks
 *
 * we assume that the BSP are configured to use these
 * address modes by default
 */

#define EPICSAddrTypeNoConvert -1

int EPICStovxWorksAddrType[] 
                = {
                VME_AM_SUP_SHORT_IO,
                VME_AM_STD_SUP_DATA,
                VME_AM_EXT_SUP_DATA,
                EPICSAddrTypeNoConvert,
                VME_AM_CSR
            };

#if CPU_FAMILY != PPC
static void initHandlerAddrList(void);
#endif

/*
 * maps logical address to physical address, but does not detect
 * two device drivers that are using the same address range
 */
static long vxDevMapAddr (epicsAddressType addrType, unsigned options,
        size_t logicalAddress, size_t size, volatile void **ppPhysicalAddress);

/*
 * a bus error safe "wordSize" read at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
static long vxDevReadProbe (unsigned wordSize, volatile const void *ptr, void *pValue);

/*
 * a bus error safe "wordSize" write at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
static long vxDevWriteProbe (unsigned wordSize, volatile void *ptr, const void *pValue);

static void *devA24Malloc(size_t size);
static void devA24Free(void *pBlock);
static long devInit(void) { return 0;}

static long vxDevConnectInterruptVME (
    unsigned vectorNumber,
    void (*pFunction)(),
    void  *parameter);

static long vxDevDisconnectInterruptVME (
    unsigned vectorNumber,
    void (*pFunction)() 
);

static long vxDevEnableInterruptLevelVME (unsigned level);

static long vxDevDisableInterruptLevelVME (unsigned level);

static int vxDevInterruptInUseVME (unsigned vectorNumber);

/*
 * used by dynamic bind in devLib.c
 */
static devLibVME vxVirtualOS = {
    vxDevMapAddr, vxDevReadProbe, vxDevWriteProbe, 
    vxDevConnectInterruptVME, vxDevDisconnectInterruptVME,
    vxDevEnableInterruptLevelVME, vxDevDisableInterruptLevelVME,
    devA24Malloc,devA24Free,devInit,vxDevInterruptInUseVME
};
devLibVME *pdevLibVME = &vxVirtualOS;

/*
 * devConnectInterruptVME
 *
 * wrapper to minimize driver dependency on vxWorks
 */
static long vxDevConnectInterruptVME (
    unsigned vectorNumber,
    void (*pFunction)(),
    void  *parameter)
{
    int status;


    if (devInterruptInUseVME(vectorNumber)) {
        return S_dev_vectorInUse; 
    }
    status = intConnect(
            (void *)INUM_TO_IVEC(vectorNumber),
            pFunction,
            (int) parameter);       
    if (status<0) {
        return S_dev_vecInstlFail;
    }

    return 0;
}

/*
 *
 *  vxDevDisconnectInterruptVME()
 *
 *  wrapper to minimize driver dependency on vxWorks
 *
 *  The parameter pFunction should be set to the C function pointer that 
 *  was connected. It is used as a key to prevent a driver from removing 
 *  an interrupt handler that was installed by another driver
 *
 */
static long vxDevDisconnectInterruptVME (
    unsigned vectorNumber,
    void (*pFunction)() 
)
{
    void (*psub)();
    int status;

#   if CPU_FAMILY == PPC
        return S_dev_vecInstlFail;
#   endif

    /*
     * If pFunction not connected to this vector
     * then they are probably disconnecting from the wrong vector
     */
    psub = isrFetch(vectorNumber);
    if(psub != pFunction){
        return S_dev_vectorNotInUse;
    }

    status = intConnect(
            (void *)INUM_TO_IVEC(vectorNumber),
            unsolicitedHandlerEPICS,
            (int) vectorNumber);        
    if(status<0){
        return S_dev_vecInstlFail;
    }

    return 0;
}

/*
 * enable VME interrupt level
 */
static long vxDevEnableInterruptLevelVME (unsigned level)
{
#   if CPU_FAMILY != I80X86 
        int s;
        s = sysIntEnable (level);
        if (s!=OK) {
            return S_dev_intEnFail;
        }
        return 0;
#   else
        return S_dev_intEnFail;
#   endif
}

/*
 * enable ISA interrupt level
 */
long devEnableInterruptLevelISA (unsigned level)
{
#   if CPU_FAMILY == I80X86
        int s;
        s = sysIntEnablePIC (level);
        if (s!=OK) {
            return S_dev_intEnFail;
        }
        return 0;
#   else
        return S_dev_intEnFail;
#   endif
}

/*
 * disable ISA interrupt level
 */
long devDisableInterruptLevelISA (unsigned level)
{
#   if CPU_FAMILY == I80X86
        int s;
        s = sysIntDisablePIC (level);
        if (s!=OK) {
            return S_dev_intEnFail;
        }
#   else
        return S_dev_intEnFail;
#   endif

    return 0;
}

/*
 * disable VME interrupt level
 */
static long vxDevDisableInterruptLevelVME (unsigned level)
{
#   if CPU_FAMILY != I80X86
        int s;
        s = sysIntDisable (level);
        if (s!=OK) {
            return S_dev_intDissFail;
        }
        return 0;
#   else
        return S_dev_intEnFail;
#   endif
}

/*
 * vxDevMapAddr ()
 */
static long vxDevMapAddr (epicsAddressType addrType, unsigned options,
            size_t logicalAddress, size_t size, volatile void **ppPhysicalAddress)
{
    long status;

    if (ppPhysicalAddress==NULL) {
        return S_dev_badArgument;
    }

    if (EPICStovxWorksAddrType[addrType] == EPICSAddrTypeNoConvert)
    {
        *ppPhysicalAddress = (void *) logicalAddress;
    }
    else
    {
        status = sysBusToLocalAdrs (EPICStovxWorksAddrType[addrType],
                        (char *) logicalAddress, (char **)ppPhysicalAddress);
        if (status) {
            return S_dev_addrMapFail;
        }
    }

    return 0;
}

/*
 * a bus error safe "wordSize" read at the specified address which returns 
 * unsuccessful status if the device isn't present
 */
static long vxDevReadProbe (unsigned wordSize, volatile const void *ptr, void *pValue)
{
    long status;

    status = vxMemProbe ((char *)ptr, VX_READ, wordSize, (char *) pValue);
    if (status!=OK) {
        return S_dev_noDevice;
    }

    return 0;
}

/*
 * a bus error safe "wordSize" write at the specified address which returns 
 * unsuccessful status if the device isn't present
 */
static long vxDevWriteProbe (unsigned wordSize, volatile void *ptr, const void *pValue)
{
    long status;

    status = vxMemProbe ((char *)ptr, VX_WRITE, wordSize, (char *) pValue);
    if (status!=OK) {
        return S_dev_noDevice;
    }

    return 0;
}

/*
 *      isrFetch()
 */
static myISR *isrFetch(unsigned vectorNumber)
{
    myISR   *psub;
    myISR   *pCISR;
    void    *pParam;
    int s;

    /*
     * fetch the handler or C stub attached at this vector
     */
    psub = (myISR *) intVecGet((FUNCPTR *)INUM_TO_IVEC(vectorNumber));

    if ( psub ) {
        /*
         * from libvxWorks/veclist.c
         *
         * checks to see if it is a C ISR
         * and if so finds the function pointer and
         * the parameter passed
         */
        s = cISRTest(psub, &pCISR, &pParam);
        if(!s){
            psub = pCISR;
        }
    }

    return psub;
}

/*
 * determine if a VME interrupt vector is in use
 */
static int vxDevInterruptInUseVME (unsigned vectorNumber)
{
#if CPU_FAMILY == PPC
    return FALSE;
#else
    {
        static int init;
        int i;
        myISR *psub;

        if (!init) {
            initHandlerAddrList();
            init = TRUE;
        }
    
        psub = isrFetch (vectorNumber);

        /*
         * its a C routine. Does it match a default handler?
         */
        for (i=0; i<NELEMENTS(defaultHandlerAddr); i++) {
            if (defaultHandlerAddr[i] == psub) {
                return FALSE;
            }
        }
    }
    return TRUE;
#   endif
}


/*
 *  unsolicitedHandlerEPICS()
 *      what gets called if they disconnect from an
 *  interrupt and an interrupt arrives on the
 *  disconnected vector
 *
 * This routine needs to be in the symbol table
 * (i.e. not static) for this code to work correctly
 */
void unsolicitedHandlerEPICS(int vectorNumber)
{
    /*
     * call logMsg() and not errMessage()
     * so we are certain that printf()
     * does not get called at interrupt level
     */
    logMsg(
        "%s: line=%d: Interrupt to EPICS disconnected vector = 0X %X",
        (int)__FILE__,
        __LINE__,
        vectorNumber,
        0,
        0,
        0);
}

#if CPU_FAMILY != PPC
/*
 *
 *  initHandlerAddrList()
 *      init list of interrupt handlers to ignore
 *
 */
static 
void initHandlerAddrList(void)
{
    int i;

    for (i=0; i<NELEMENTS(defaultHandlerNames); i++) {
        defaultHandlerAddr[i] = epicsFindSymbol(defaultHandlerNames[i]);
        if(!defaultHandlerAddr[i]) {
            errPrintf(
                S_dev_internal,
                __FILE__,
                __LINE__,
                "initHandlerAddrList() %s not in sym table",
                defaultHandlerNames[i]);
        }
    }
}
#endif

/******************************************************************************
 *
 * Routines to use to allocate and free memory present in the A24 region.
 *
 ******************************************************************************/

static void * (*A24MallocFunc)(size_t) = NULL;
static void   (*A24FreeFunc)(void *)     = NULL;

static void *devA24Malloc(size_t size)
{
    static int    UsingBSP = 0;
    void        *ret;
    
    if (A24MallocFunc == NULL)
    {
        /* See if the sysA24Malloc() function is present. */
        A24MallocFunc = epicsFindSymbol("sysA24Malloc");
        if(!A24MallocFunc) {
            A24MallocFunc = malloc;
            A24FreeFunc   = free;
        } else {
            A24FreeFunc = epicsFindSymbol("sysA24Free");
            if(!A24FreeFunc) {
              /* That's strange... we have malloc, but no free! */
                A24MallocFunc = malloc;
                A24FreeFunc   = free;
            } else {
                UsingBSP = 1;
            }
        }
    }
    ret = A24MallocFunc(size);
    
    if ((ret == NULL) && (UsingBSP))
        errMessage(S_dev_noMemory, "devLibA24Malloc ran out of A24 memory, try sysA24MapRam(size)");
    
    return(ret);
}

static void devA24Free(void *pBlock)
{
    A24FreeFunc(pBlock);
}
