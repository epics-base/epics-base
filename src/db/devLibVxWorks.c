/* 
 * devLibVxWorks.c
 * @(#)$Id$
 *
 * Archictecture dependent support for common device driver resources 
 *
 *      Author: Jeff Hill 
 *      Date: 10-30-98	
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
 */

static char *sccsID = "@(#) $Id$";


#include <vxWorks.h>
#include <types.h>
#include <iv.h>
#include <vme.h>
#include <sysLib.h>
#include <memLib.h>
#include <intLib.h>
#include <logLib.h>
#include <vxLib.h>

#include "devLib.h"
#include "epicsDynLink.h"

/*
 * A list of the names of the unexpected interrupt handlers
 * ( some of these are provided by wrs )
 */
LOCAL char	*defaultHandlerNames[] = {
			"_excStub",
			"_excIntStub",
			"_unsolicitedHandlerEPICS"};

typedef void	myISR (void *pParam);
LOCAL myISR	*defaultHandlerAddr[NELEMENTS(defaultHandlerNames)];

LOCAL myISR	*isrFetch(unsigned vectorNumber);

/*
 * this routine needs to be in the symbol table
 * for this code to work correctly
 */
void unsolicitedHandlerEPICS(int vectorNumber);

/*
 * this is in veclist.c
 */
int cISRTest(void (*)(), void (**)(), void **);

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

LOCAL void initHandlerAddrList(void);

/*
 * maps logical address to physical address, but does not detect
 * two device drivers that are using the same address range
 */
LOCAL long vxDevMapAddr (epicsAddressType addrType, unsigned options,
		size_t logicalAddress, size_t size, volatile void **ppPhysicalAddress);

/*
 * a bus error safe "wordSize" read at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
long vxDevReadProbe (unsigned wordSize, volatile const void *ptr, void *pValue);

/*
 * a bus error safe "wordSize" write at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
long vxDevWriteProbe (unsigned wordSize, volatile void *ptr, const void *pValue);

/*
 * used by dynamic bind in devLib.c
 */
const struct devLibVirtualOS devLibVirtualOS = 
	{vxDevMapAddr, vxDevReadProbe, vxDevWriteProbe, 
	devConnectInterruptVME, devDisconnectInterruptVME};

#define SUCCESS 0

/*
 * devConnectInterruptVME
 *
 * wrapper to minimize driver dependency on vxWorks
 */
long devConnectInterruptVME (
	unsigned vectorNumber,
	void (*pFunction)(),
	void  *parameter)
{
	int	status;


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

	return SUCCESS;
}

/*
 *
 *	devDisconnectInterruptVME()
 *
 *	wrapper to minimize driver dependency on vxWorks
 *
 * 	The parameter pFunction should be set to the C function pointer that 
 * 	was connected. It is used as a key to prevent a driver from removing 
 * 	an interrupt handler that was installed by another driver
 *
 */
long devDisconnectInterruptVME (
	unsigned vectorNumber,
	void (*pFunction)() 
)
{
	void (*psub)();
	int	status;

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

	return SUCCESS;
}

/*
 * devEnableInterruptLevel()
 *
 * wrapper to minimize driver dependency on vxWorks
 */
long devEnableInterruptLevel(
epicsInterruptType      intType,
unsigned                level)
{
	int	s;

	switch (intType) {
	case intVME:
	case intVXI:
		s = sysIntEnable (level);
		if(s!=OK){
			return S_dev_intEnFail;
		}
		break;
	case intISA:
#		if CPU == I80386
			s = sysIntEnablePIC (level);
			if (s!=OK) {
				return S_dev_intEnFail;
			}
#		endif
	default:
		return S_dev_uknIntType;
	}

	return SUCCESS;
}

/*
 * devDisableInterruptLevel()
 *
 * wrapper to minimize driver dependency on vxWorks
 */
long    devDisableInterruptLevel (
epicsInterruptType      intType,
unsigned                level)
{
	int s;

	switch (intType) {
	case intVME:
	case intVXI:
		s = sysIntDisable (level);
		if(s!=OK){
			return S_dev_intDissFail;
		}
		break;
	case intISA:
#		if CPU == I80386
			s = sysIntDisablePIC (level);
			if (s!=OK) {
				return S_dev_intEnFail;
			}
#		endif
	default:
		return S_dev_uknIntType; 
	}

	return SUCCESS;
}

/*
 * vxDevMapAddr ()
 */
LOCAL long vxDevMapAddr (epicsAddressType addrType, unsigned options,
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

	return SUCCESS;
}

/*
 * a bus error safe "wordSize" read at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
long vxDevReadProbe (unsigned wordSize, volatile const void *ptr, void *pValue)
{
	long status;

	/*
	 * this global variable exists in the nivxi library
	 */
	status = vxMemProbe ((char *)ptr, VX_READ, wordSize, (char *) pValue);
	if (status!=OK) {
		return S_dev_noDevice;
	}

	return SUCCESS;
}

/*
 * a bus error safe "wordSize" write at the specified address which returns 
 * unsuccessful status if the device isnt present
 */
long vxDevWriteProbe (unsigned wordSize, volatile void *ptr, const void *pValue)
{
	long status;

	/*
	 * this global variable exists in the nivxi library
	 */
	status = vxMemProbe ((char *)ptr, VX_READ, wordSize, (char *) pValue);
	if (status!=OK) {
		return S_dev_noDevice;
	}

	return SUCCESS;
}

/*
 *      isrFetch()
 */
LOCAL myISR *isrFetch(unsigned vectorNumber)
{
	myISR	*psub;
	myISR	*pCISR;
	void	*pParam;
	int	s;

	/*
	 * fetch the handler or C stub attached at this vector
	 */
	psub = (myISR *) intVecGet((FUNCPTR *)INUM_TO_IVEC(vectorNumber));

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

	return psub;
}

/*
 * determine if a VME interrupt vector is in use
 */
int devInterruptInUseVME (unsigned vectorNumber)
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

	return TRUE;
}


/*
 *	unsolicitedHandlerEPICS()
 *     	what gets called if they disconnect from an
 *	interrupt and an interrupt arrives on the
 *	disconnected vector
 *
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
		NULL,
		NULL,
		NULL);
}

/*
 *
 *	initHandlerAddrList()
 *      init list of interrupt handlers to ignore
 *
 */
LOCAL 
void initHandlerAddrList(void)
{
	int i;
	SYM_TYPE type;
	int status;

	for (i=0; i<NELEMENTS(defaultHandlerNames); i++) {
		status = symFindByNameEPICS (sysSymTbl,
								defaultHandlerNames[i],
								(char **)&defaultHandlerAddr[i],
								&type);
		if (status != OK) {
			errPrintf(
				S_dev_internal,
				__FILE__,
				__LINE__,
				"initHandlerAddrList() %s not in sym table",
				defaultHandlerNames[i]);
		}
	}
}

