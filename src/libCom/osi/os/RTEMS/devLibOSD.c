/*  devLibRTEMS.c */
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
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
#include "devLib.h"
#include <epicsInterrupt.h>

#if defined(__PPC__) || defined(__mcf528x__)

#if defined(__PPC__)
#include <bsp/VME.h>
#include <bsp/bspExt.h>
#endif


typedef void    myISR (void *pParam);

LOCAL myISR *isrFetch(unsigned vectorNumber, void **parg);

/*
 * this routine needs to be in the symbol table
 * for this code to work correctly
 */
void unsolicitedHandlerEPICS(int vectorNumber);

LOCAL myISR *defaultHandlerAddr[]={
		(myISR*)unsolicitedHandlerEPICS,
};

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
                EPICSAddrTypeNoConvert
            };

/*
 * maps logical address to physical address, but does not detect
 * two device drivers that are using the same address range
 */
LOCAL long rtmsDevMapAddr (epicsAddressType addrType, unsigned options,
        size_t logicalAddress, size_t size, volatile void **ppPhysicalAddress);

/*
 * a bus error safe "wordSize" read at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
long rtmsDevReadProbe (unsigned wordSize, volatile const void *ptr, void *pValue);

/*
 * a bus error safe "wordSize" write at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
long rtmsDevWriteProbe (unsigned wordSize, volatile void *ptr, const void *pValue);

/* RTEMS specific init */

/*devA24Malloc and devA24Free are not implemented*/
LOCAL void *devA24Malloc(size_t size) { return 0;}
LOCAL void devA24Free(void *pBlock) {};
LOCAL long rtmsDevInit(void);

/*
 * used by bind in devLib.c
 */
const struct devLibVirtualOS devLibRTEMSOS = 
    {rtmsDevMapAddr, rtmsDevReadProbe, rtmsDevWriteProbe, 
    devConnectInterruptVME, devDisconnectInterruptVME,
    devEnableInterruptLevelVME, devDisableInterruptLevelVME,
    devA24Malloc,devA24Free,rtmsDevInit};
devLibVirtualOS *pdevLibVirtualOS = &devLibRTEMSOS;

/* RTEMS specific initialization */
LOCAL long
rtmsDevInit(void)
{
	/* assume the vme bridge has been initialized by bsp */
	/* init BSP extensions [memProbe etc.] */
	return bspExtInit();
}

/*
 * devConnectInterruptVME
 *
 * wrapper to minimize driver dependency on OS
 */
long devConnectInterruptVME (
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
long devDisconnectInterruptVME (
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
long devEnableInterruptLevelVME (unsigned level)
{
		return BSP_enableVME_int_lvl(level);
}

/*
 * disable VME interrupt level
 */
long devDisableInterruptLevelVME (unsigned level)
{
		return BSP_disableVME_int_lvl(level);
}

/*
 * rtmsDevMapAddr ()
 */
LOCAL long rtmsDevMapAddr (epicsAddressType addrType, unsigned options,
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

/*
 * a bus error safe "wordSize" read at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
long rtmsDevReadProbe (unsigned wordSize, volatile const void *ptr, void *pValue)
{
    long status;

    /*
     * this global variable exists in the nivxi library
     */
    status = bspExtMemProbe ((void*)ptr, 0/*read*/, wordSize, pValue);
    if (status!=RTEMS_SUCCESSFUL) {
        return S_dev_noDevice;
    }

    return 0;
}

/*
 * a bus error safe "wordSize" write at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
long rtmsDevWriteProbe (unsigned wordSize, volatile void *ptr, const void *pValue)
{
    long status;

    /*
     * this global variable exists in the nivxi library
     */
    status = bspExtMemProbe ((void*)ptr, 1/*write*/, wordSize, (void*)pValue);
    if (status!=RTEMS_SUCCESSFUL) {
        return S_dev_noDevice;
    }

    return 0;
}

/*
 *      isrFetch()
 */
LOCAL myISR *isrFetch(unsigned vectorNumber, void **parg)
{
    /*
     * fetch the handler or C stub attached at this vector
     */
    return (myISR *) BSP_getVME_isr(vectorNumber,parg);
}

/*
 * determine if a VME interrupt vector is in use
 */
int devInterruptInUseVME (unsigned vectorNumber)
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
void unsolicitedHandlerEPICS(int vectorNumber)
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

#endif /* defined(__PPC__) && defined(mpc750) */

/*
 * Some vxWorks convenience routines
 */
void
bcopyLongs(char *source, char *destination, int nlongs)
{
    const long *s = (long *)source;
    long *d = (long *)destination;
    while(nlongs--)
        *d++ = *s++;
}
