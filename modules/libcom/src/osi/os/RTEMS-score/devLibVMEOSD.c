/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*      RTEMS port by Till Straumann, <strauman@slac.stanford.edu>
 *            3/2002
 *
 */

#include <epicsStdio.h>
#include <epicsExit.h>
#include <rtems.h>
#include <bsp.h>
#include "devLibVME.h"
#include <epicsInterrupt.h>
#include <epicsMMIO.h>
#include <string.h>

#if defined(__PPC__) || defined(__mcf528x__)
/* EPICS VME support requires various routines to be provided by the
 * BSP. Most MVME PowerPC BSPs provide them, plus the uC5282.
 */

#if defined(__PPC__)
#include <bsp/VME.h>
#endif


typedef void    myISR (void *pParam);

static myISR *isrFetch(unsigned vectorNumber, void **parg);

/*
 * this routine needs to be in the symbol table
 * for this code to work correctly
 */
static void unsolicitedHandlerEPICS(int vectorNumber);

static myISR *defaultHandlerAddr[]={
    (myISR*)unsolicitedHandlerEPICS,
};

/*
 * Make sure that the CR/CSR addressing mode is defined.
 * (it may not be in some BSPs).
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

/*
 * maps logical address to physical address, but does not detect
 * two device drivers that are using the same address range
 */
static long rtemsDevMapAddr (epicsAddressType addrType, unsigned options,
        size_t logicalAddress, size_t size, volatile void **ppPhysicalAddress);

/*
 * a bus error safe "wordSize" read at the specified address which returns
 * unsuccessful status if the device isn't present
 */
static long rtemsDevReadProbe (unsigned wordSize, volatile const void *ptr, void *pValue);

/*
 * a bus error safe "wordSize" write at the specified address which returns
 * unsuccessful status if the device isn't present
 */
static long rtemsDevWriteProbe (unsigned wordSize, volatile void *ptr, const void *pValue);

static long rtemsDevConnectInterruptVME (
    unsigned vectorNumber,
    void (*pFunction)(),
    void  *parameter);

static long rtemsDevDisconnectInterruptVME (
    unsigned vectorNumber,
    void (*pFunction)()
);

static long rtemsDevEnableInterruptLevelVME (unsigned level);

static long rtemsDevDisableInterruptLevelVME (unsigned level);

static int rtemsDevInterruptInUseVME (unsigned vectorNumber);

/* RTEMS specific init */

/*devA24Malloc and devA24Free are not implemented*/
static void *devA24Malloc(size_t size) { return 0;}
static void devA24Free(void *pBlock) {};
static long rtemsDevInit(void);

/*
 * used by bind in devLib.c
 */
static devLibVME rtemsVirtualOS = {
    rtemsDevMapAddr, rtemsDevReadProbe, rtemsDevWriteProbe,
    rtemsDevConnectInterruptVME, rtemsDevDisconnectInterruptVME,
    rtemsDevEnableInterruptLevelVME, rtemsDevDisableInterruptLevelVME,
    devA24Malloc,devA24Free,rtemsDevInit,rtemsDevInterruptInUseVME
};
devLibVME *pdevLibVME = &rtemsVirtualOS;

/* RTEMS specific initialization */
static long
rtemsDevInit(void)
{
    /* assume the vme bridge has been initialized by bsp */
    return 0;
}

/*
 * devConnectInterruptVME
 *
 * wrapper to minimize driver dependency on OS
 */
static long rtemsDevConnectInterruptVME (
    unsigned vectorNumber,
    void (*pFunction)(),
    void  *parameter)
{
    int status;


    if (devInterruptInUseVME(vectorNumber)) {
        return S_dev_vectorInUse;
    }
    status = BSP_installVME_isr(
            vectorNumber,
            pFunction,
            parameter);
    if (status) {
        return S_dev_vecInstlFail;
    }

    return 0;
}

/*
 *
 *  devDisconnectInterruptVME()
 *
 *  wrapper to minimize driver dependency on OS
 *
 *  The parameter pFunction should be set to the C function pointer that
 *  was connected. It is used as a key to prevent a driver from removing
 *  an interrupt handler that was installed by another driver
 *
 */
static long rtemsDevDisconnectInterruptVME (
    unsigned vectorNumber,
    void (*pFunction)()
)
{
    void (*psub)();
    void  *arg;
    int status;

    /*
     * If pFunction not connected to this vector
     * then they are probably disconnecting from the wrong vector
     */
    psub = isrFetch(vectorNumber, &arg);
    if(psub != pFunction){
        return S_dev_vectorNotInUse;
    }

    status = BSP_removeVME_isr(
        vectorNumber,
        psub,
        arg) ||
     BSP_installVME_isr(
        vectorNumber,
        (BSP_VME_ISR_t)unsolicitedHandlerEPICS,
        (void*)vectorNumber);
    if(status){
        return S_dev_vecInstlFail;
    }

    return 0;
}

/*
 * enable VME interrupt level
 */
static long rtemsDevEnableInterruptLevelVME (unsigned level)
{
    return BSP_enableVME_int_lvl(level);
}

/*
 * disable VME interrupt level
 */
static long rtemsDevDisableInterruptLevelVME (unsigned level)
{
    return BSP_disableVME_int_lvl(level);
}

/*
 * rtemsDevMapAddr ()
 */
static long rtemsDevMapAddr (epicsAddressType addrType, unsigned options,
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
        status = BSP_vme2local_adrs(EPICStovxWorksAddrType[addrType],
                        logicalAddress, (unsigned long *)ppPhysicalAddress);
        if (status) {
            return S_dev_addrMapFail;
        }
    }

    return 0;
}

#if defined(__m68k__)
/* All RTEMS m68k BSPs define this, m68k/shared/[misc/]memProbe.c */
extern
rtems_status_code bspExtMemProbe(void *addr, int write, int size, void *pval);
#else

static
rtems_status_code bspExtMemProbe(void *addr, int write, int size, void *pval)
{
    rtems_interrupt_level flags;
    rtems_status_code ret = RTEMS_SUCCESSFUL;
    epicsUInt32 val;

    /* bspExt allows caller to write uninitialized values, we don't */
    if(write && !pval)
        return RTEMS_INVALID_NUMBER;

    switch(size) {
    case 1:
    case 2:
    case 4:
        break;
    default:
        return RTEMS_INVALID_SIZE;
    }

    if(write)
        memcpy(&val, pval, size);

    rtems_interrupt_disable(flags);
    _BSP_clear_hostbridge_errors(0,1);

    if(!write) {
        switch(size) {
        case 1: val = ioread8(addr)<<24; break;
        case 2: val = nat_ioread16(addr)<<16; break;
        case 4: val = nat_ioread32(addr); break;
        }
    } else {
        switch(size) {
        case 1: iowrite8(addr, val>>24); break;
        case 2: nat_iowrite16(addr, val>>16); break;
        case 4: nat_iowrite32(addr, val); break;
        }
    }

    ret = _BSP_clear_hostbridge_errors(0,1);
    rtems_interrupt_enable(flags);

    if(!write && pval)
        memcpy(pval, &val, size);

    return ret;
}
#endif

/*
 * a bus error safe "wordSize" read at the specified address which returns
 * unsuccessful status if the device isn't present
 */
static long rtemsDevReadProbe (unsigned wordSize, volatile const void *ptr, void *pValue)
{
    long status;

    status = bspExtMemProbe ((void*)ptr, 0/*read*/, wordSize, pValue);
    if (status!=RTEMS_SUCCESSFUL) {
        return S_dev_noDevice;
    }

    return 0;
}

/*
 * a bus error safe "wordSize" write at the specified address which returns
 * unsuccessful status if the device isn't present
 */
static long rtemsDevWriteProbe (unsigned wordSize, volatile void *ptr, const void *pValue)
{
    long status;

    status = bspExtMemProbe ((void*)ptr, 1/*write*/, wordSize, (void*)pValue);
    if (status!=RTEMS_SUCCESSFUL) {
        return S_dev_noDevice;
    }

    return 0;
}

/*
 *      isrFetch()
 */
static myISR *isrFetch(unsigned vectorNumber, void **parg)
{
    /*
     * fetch the handler or C stub attached at this vector
     */
    return (myISR *) BSP_getVME_isr(vectorNumber,parg);
}

/*
 * determine if a VME interrupt vector is in use
 */
static int rtemsDevInterruptInUseVME (unsigned vectorNumber)
{
    int i;
    myISR *psub;
    void *arg;

    psub = isrFetch (vectorNumber,&arg);

    if (!psub)
        return FALSE;

    /*
     * its a C routine. Does it match a default handler?
     */
    for (i=0; i<NELEMENTS(defaultHandlerAddr); i++) {
        if (defaultHandlerAddr[i] == psub) {
            return FALSE;
        }
    }

    return TRUE;
}


/*
 *  unsolicitedHandlerEPICS()
 *      what gets called if they disconnect from an
 *  interrupt and an interrupt arrives on the
 *  disconnected vector
 *
 *  NOTE: RTEMS may pass additional arguments - hope
 *        this doesn't disturb this handler...
 *
 *  A cleaner way would be having a OS dependent
 *  macro to declare handler prototypes...
 *
 */
static void unsolicitedHandlerEPICS(int vectorNumber)
{
    /*
     * call epicInterruptContextMessage()
     * and not errMessage()
     * so we are certain that printf()
     * does not get called at interrupt level
     *
     * NOTE: current RTEMS implementation only
     *       allows a static string to be passed
     */
    epicsInterruptContextMessage(
        "Interrupt to EPICS disconnected vector"
        );
}

#else /* !defined(__PPC__) && !defined(__mcf528x__) */

/* No known VME interface here, provide a dummy */
devLibVME *pdevLibVME;

#endif /* defined(__PPC__) || defined(__mcf528x__) */

/*
 * Some convenience routines
 */
void
bcopyLongs(char *source, char *destination, int nlongs)
{
    const long *s = (long *)source;
    long *d = (long *)destination;
    while(nlongs--)
        *d++ = *s++;
}
