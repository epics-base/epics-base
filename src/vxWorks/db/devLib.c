/* devLib.c - support for allocation of common device resources */
/* @(#)$Id$*/

/*
 *	Original Author: Marty Kraimer
 *      Author:          Jeff Hill 
 *      Date:		 03-10-93	
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
 * Modification Log:
 * -----------------
 * .01  03-10-93        joh     original
 * .02  03-18-93        joh    	index address alloc array by fundamental type 
 * .03  03-23-93        joh     changed input parameter to be a fund
 *                              address type in favor of the argument
 *                              that the BSP will be reconfigured
 *                              to use an EPICS standard address
 *                              mode
 * .04	04-08-93	joh	made unsolicitedHandlerEPICS() external
 * .05	04-08-92	joh	better diagnostic if we cant find
 *				a default interrupt handler
 * .06  05-06-93        joh     added new parameter to devDisconnectInterrupt().
 *                              See comment below.
 * .07  05-28-93	joh	Added block probe routines
 * .08  05-28-93	joh	Added an argument to devRegisterAddress() 
 * .09  05-28-93	joh	Added devAddressMap() 
 * .10  06-14-93        joh     Added devAllocAddress()
 * .11  02-21-95        joh    	Fixed warning messages 
 *
 * NOTES:
 * .01	06-14-93 	joh	needs devAllocInterruptVector() routine
 */

static char *sccsID = "@(#) $Id$";


#include	<vxWorks.h>
#include        <types.h>
#include        <iv.h>
#include        <vme.h>
#include	<lstLib.h>
#include	<sysLib.h>
#include 	<sysSymTbl.h>
#include	<memLib.h>
#include        <intLib.h>
#include	<logLib.h>
#include 	<string.h>
#include 	<stdioLib.h>
#include	<stdlib.h>

#include	<fast_lock.h>
#define devLibGlobal
#include	<devLib.h>


LOCAL LIST addrAlloc[atLast];
LOCAL LIST addrFree[atLast];

LOCAL void	*addrLast[atLast] = {
			(void *) 0xffff,
			(void *) 0xffffff,
			(void *) 0xffffffff};

LOCAL long	addrFail[atLast] = {
			S_dev_badA16,
			S_dev_badA24,
			S_dev_badA32};

LOCAL FAST_LOCK	addrListLock;
LOCAL char	addrListInit;
 
typedef struct{
	NODE		node;
	const char	*pOwnerName;
        char		*pFirst;
	char		*pLast;
}rangeItem;

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

/*
 * These routines are not exported
 */
LOCAL void	initHandlerAddrList(void);
LOCAL int 	vectorInUse(unsigned vectorNumber);
LOCAL long	initAddrList(void);
LOCAL long    	addrVerify(epicsAddressType addrType, void *address);
LOCAL myISR	*isrFetch(unsigned vectorNumber);
LOCAL long 	blockFind(
			epicsAddressType	addrType,
			char			*pBlockFirst,
			char			*pBlockLast,
			/* size needed */
			unsigned long		size,
			/* n ls bits zero in base addr */
			unsigned 		alignment,
			/* base address found */
			char			**ppBase);
LOCAL long 	report_conflict(
			epicsAddressType        addrType,
			char			*pFirst,
			char			*pLast,
			const char		*pOwnerName);
LOCAL long    	devInsertAddress(
			LIST			*pRangeList,
			rangeItem		*pNewRange);
LOCAL long 	devListAddressMap(
			LIST 			*pRangeList);
LOCAL long    	devCombineAdjacentBlocks(
			LIST		*pRangeList,
			rangeItem	*pRange);
LOCAL long 	devInstallAddr(
			rangeItem		*pRange, 
			const char		*pOwnerName,
			epicsAddressType        addrType,
			char			*pFirst,
			char			*pLast,
			void                    **pLocalAddress);
LOCAL long 	blockDivide(
			epicsAddressType	addrType,
			char			*pBlockFirst,
			char			*pBlockLast,
			/* base address found */
			char			**ppBase,	
			unsigned long		requestSize
);
LOCAL long 	blockProbe(
			epicsAddressType	addrType,
			char			*pFirst,
			char			*pLast
);
long locationProbe(
			epicsAddressType	addrType,
			char			*pLocation
);

/*
 * this routine needs to be in the symbol table
 * for this code to work correctly
 */
void		unsolicitedHandlerEPICS(int vectorNumber);
/*
 * this is in veclist.c
 */
int     	cISRTest(void (*)(), void (**)(), void **);

#define SUCCESS 0



/*
 *
 *      devConnectInterrupt
 *
 *	coded to support other interrupting types in the future
 *
 *	wrapper to minimize driver dependency on vxWorks
 */
long    devConnectInterrupt(
epicsInterruptType      intType,
unsigned                vectorNumber,
void                    (*pFunction)(),
void                    *parameter)
{
	int	status;

	switch(intType){
	case intCPU:
	case intVME:
	case intVXI:
		if(vectorInUse(vectorNumber)){
			return S_dev_vectorInUse; 
		}
		status = intConnect(
				(void *)INUM_TO_IVEC(vectorNumber),
				pFunction,
				(int) parameter);		
		if(status<0){
			return S_dev_vxWorksVecInstlFail;
		}
		break;
	default:
		return S_dev_uknIntType;
	}

	return SUCCESS;
}


/*
 *
 *	devDisconnectInterrupt()
 *
 *	wrapper to minimize driver dependency on vxWorks
 *
 * 	The parameter pFunction should be set to the C function pointer that 
 * 	was connected. It is used as a key to prevent a driver from removing 
 * 	an interrupt handler that was installed by another driver
 *
 */
long    devDisconnectInterrupt(
epicsInterruptType      intType,
unsigned                vectorNumber,
void			(*pFunction)() 
)
{
	void	(*psub)();
	int	status;

	/*
	 * If pFunction not connected to this vector
	 * then they are probably disconnecting from the wrong vector
	 */
	psub = isrFetch(vectorNumber);
	if(psub != pFunction){
		return S_dev_vectorNotInUse;
	}

	switch(intType){
	case intCPU:
	case intVME:
	case intVXI:
		status = intConnect(
				(void *)INUM_TO_IVEC(vectorNumber),
				unsolicitedHandlerEPICS,
				(int) vectorNumber);		
		if(status<0){
			return S_dev_vxWorksVecInstlFail;
		}
		break;
	default:
		return S_dev_uknIntType;
	}

	return SUCCESS;
}


/*
 *
 *      devEnableInterruptLevel()
 *
 *	wrapper to minimize driver dependency on vxWorks
 *
 */
long    devEnableInterruptLevel(
epicsInterruptType      intType,
unsigned                level)
{
	int	s;

	switch(intType){
	case intCPU:
	case intVME:
	case intVXI:
		s = sysIntEnable(level);
		if(s<0){
			return S_dev_vxWorksIntEnFail;
		}
		break;
	default:
		return S_dev_uknIntType;
	}

	return SUCCESS;
}


/*
 *
 *      devDisableInterruptLevel()
 *
 *	wrapper to minimize driver dependency on vxWorks
 *
 */
long    devDisableInterruptLevel(
epicsInterruptType      intType,
unsigned                level)
{
	int s;

	switch(intType){
	case intCPU:
	case intVME:
	case intVXI:
		s = sysIntDisable(level);
		if(s<0){
                        return S_dev_vxWorksIntDissFail;
		}
		break;
	default:
		return S_dev_uknIntType; 
	}

	return SUCCESS;
}


/*
 *
 *      devRegisterAddress()
 *
 *
 */
long    devRegisterAddress(
const char		*pOwnerName,
epicsAddressType        addrType,
void                    *baseAddress,
unsigned long		size,
void                    **pLocalAddress)
{
	char		*pFirst;
	char		*pLast;
        rangeItem      	*pRange;
	long		s;

	if(!addrListInit){
		s = initAddrList();
		if(s){
			return s;
		}
	}

	s = addrVerify(addrType, (void *) size);
	if(s){
		return s;
	}

	if(size == 0){
		return S_dev_lowValue;
	}
 
	pFirst = (char *) baseAddress;
	pLast = pFirst + size - 1;

	FASTLOCK(&addrListLock);
        pRange = (rangeItem *) addrFree[addrType].node.next;
        while(pRange){
 
		if(pRange->pFirst > pLast){
			pRange = NULL;
			break;
		}

		if(pRange->pFirst <= pFirst && pRange->pLast >= pLast){
			break;
		}

		pRange = (rangeItem *) pRange->node.next;
	}
	FASTUNLOCK(&addrListLock);

	if(!pRange){
		s = report_conflict(addrType, pFirst, pLast, pOwnerName);
		if(s){
			return s;
		}
		else{
			return S_dev_internal;
		}
	}

	s = devInstallAddr(
			pRange,
			pOwnerName,
			addrType,
			pFirst,
			pLast,
			pLocalAddress);
	return s;
}


/*
 *
 *	devInstallAddr()
 *
 */
LOCAL long devInstallAddr(
rangeItem		*pRange, /* item on the free list to be split */
const char		*pOwnerName,
epicsAddressType        addrType,
char			*pFirst,
char			*pLast,
void			**ppLocalAddress)
{
	rangeItem 	*pNewRange;
	int		s;

	if(ppLocalAddress){
		char	*pAddr;
		int 	s1;
		int 	s2;

		s1 = sysBusToLocalAdrs(
	               	        EPICStovxWorksAddrType[addrType],
	                        pLast,
	                        &pAddr);
        	s2 = sysBusToLocalAdrs(
	               	        EPICStovxWorksAddrType[addrType],
	                        pFirst,
	                        &pAddr);
		if(s1 || s2){
			errPrintf(
				S_dev_vxWorksAddrMapFail,
				__FILE__,
				__LINE__,
				"%s base=0X %X size = 0X %X",
				epicsAddressTypeName[addrType],
				pFirst,
				pLast-pFirst+1);
               	 	return S_dev_vxWorksAddrMapFail;
		}

		*ppLocalAddress = (void *) pAddr;
	}

	/*
	 * split the item on the free list
	 * (when required)
	 */
	if(pRange->pFirst == pFirst && pRange->pLast == pLast){
		FASTLOCK(&addrListLock);
		lstDelete(&addrFree[addrType], &pRange->node);
		FASTUNLOCK(&addrListLock);
		free((void *)pRange);
	}
	else if(pRange->pFirst == pFirst){
		pRange->pFirst = pLast+1;
	}
	else if(pRange->pLast == pLast){
		pRange->pLast = pFirst-1;
	}
	else{

		pNewRange = (rangeItem *) malloc(sizeof(*pRange));
		if(!pNewRange){
			return S_dev_noMemory;
		}
		pNewRange->pFirst = pLast+1;
		pNewRange->pLast = pRange->pLast;
		pNewRange->pOwnerName = "<fragmented block>";
		pRange->pLast = pFirst-1;
		/*
		 * add the node after the old item on the free list
		 * (blocks end up ordered by address)
		 */
		FASTLOCK(&addrListLock);
		lstInsert(&addrFree[addrType], &pRange->node, &pNewRange->node);
		FASTUNLOCK(&addrListLock);
	}

        /*
         * allocate a new address range entry and add it to
         * the list
         */
        pNewRange = (rangeItem *)calloc(1,sizeof(*pRange));
	if(!pNewRange){
		return S_dev_noMemory;
	}

	pNewRange->pFirst = pFirst;
	pNewRange->pLast = pLast;
	pNewRange->pOwnerName = pOwnerName;

	s = devInsertAddress(&addrAlloc[addrType], pNewRange);
	if(s){
		free((void *)pNewRange);
		return s;
	}

	return SUCCESS;
}


/*
 *
 * report_conflict()
 *
 *
 */
LOCAL long report_conflict(
epicsAddressType        addrType,
char			*pFirst,
char			*pLast,
const char		*pOwnerName
)
{
        rangeItem      	*pRange;

        pRange = (rangeItem *) addrAlloc[addrType].node.next;
        while(pRange){
 
		if(pRange->pFirst <= pFirst && pRange->pLast >= pFirst){
			break;
		}
		if(pRange->pFirst <= pLast && pRange->pLast >= pLast){
			break;
		}
		if(pRange->pFirst > pLast){
			pRange = NULL;
			break;
		}

		pRange = (rangeItem *) pRange->node.next;
	}
	
	if(pRange){
		errPrintf(
				S_dev_addressOverlap,
				__FILE__,
				__LINE__,
				"%10s 0X %08X - %08X Requested by %s",
				epicsAddressTypeName[addrType],
				pFirst,
				pLast,
				pOwnerName);

		errPrintf(
				S_dev_identifyOverlap,
				__FILE__,
				__LINE__,
				"%10s 0X %08X - %08X Owned by %s",
				epicsAddressTypeName[addrType],
				pRange->pFirst,
				pRange->pLast,
				pRange->pOwnerName);

		return S_dev_addressOverlap;
        }

	return S_dev_internal;
}


/*
 *
 *	devUnregisterAddress()
 *
 */
long devUnregisterAddress(
epicsAddressType        addrType,
void                    *baseAddress,
const char		*pOwnerName)
{
	char		*charAddress = (char *) baseAddress;
        rangeItem       *pRange;
	int 		s;

	if(!addrListInit){
		s = initAddrList();
		if(s){
			return s;
		}
	}

	s = addrVerify(addrType, charAddress);
	if(s != SUCCESS){
		return s;
	}

	FASTLOCK(&addrListLock);
        pRange = (rangeItem *) addrAlloc[addrType].node.next;
        while(pRange){
		if(pRange->pFirst == charAddress){
			break;
		}
		if(pRange->pFirst > charAddress){
			pRange = NULL;
			break;
		}
		pRange = (rangeItem *) pRange->node.next;
	}
	FASTUNLOCK(&addrListLock);
	
	if(!pRange){
		return S_dev_addressNotFound;
	}

	if(strcmp(pOwnerName,pRange->pOwnerName)){
		s = S_dev_addressOverlap;
		errPrintf(
			s, 
			__FILE__,
			__LINE__,
	"unregister address for %s at 0X %X failed because %s owns it",
			pOwnerName,
			charAddress,
			pRange->pOwnerName);
		return s;
	}	

	FASTLOCK(&addrListLock);
	lstDelete(
		&addrAlloc[addrType],
		&pRange->node);
	FASTUNLOCK(&addrListLock);

	pRange->pOwnerName = "<released fragment>";
	s = devInsertAddress(&addrFree[addrType], pRange);
	if(s){
		free((void *)pRange);
		errMessage(s, "Allocated Device Address Leak");
		return s;
	}
	s = devCombineAdjacentBlocks(&addrFree[addrType], pRange);
	if(s){
		errMessage(s, NULL);
		return s;
	}

	return SUCCESS;
}


/*
 *
 *      devCombineAdjacentBlocks()
 *
 *
 */
LOCAL long    	devCombineAdjacentBlocks(
LIST		*pRangeList,
rangeItem	*pRange)
{
	rangeItem	*pBefore;
	rangeItem	*pAfter;

	pBefore = (rangeItem *) pRange->node.previous;
	pAfter = (rangeItem *) pRange->node.next;

	/*
	 * combine adjacent blocks
	 */
	if(pBefore){
		if(pBefore->pLast == pRange->pFirst-1){
			FASTLOCK(&addrListLock);
			pRange->pFirst = pBefore->pFirst;
			lstDelete(pRangeList, &pBefore->node);
			FASTUNLOCK(&addrListLock);
			free((void *)pBefore);
		}
	}

	if(pAfter){
		if(pAfter->pFirst-1 == pRange->pLast){
			FASTLOCK(&addrListLock);
			pRange->pLast = pAfter->pLast;
			lstDelete(pRangeList, &pAfter->node);
			FASTUNLOCK(&addrListLock);
			free((void *)pAfter);
		}
	}

	return SUCCESS;
}


/*
 *
 *      devInsertAddress()
 *
 *
 */
LOCAL long    devInsertAddress(
LIST		*pRangeList,
rangeItem	*pNewRange)
{
	rangeItem	*pBefore;
	rangeItem	*pAfter;

	FASTLOCK(&addrListLock);
        pAfter = (rangeItem *) pRangeList->node.next;
	while(pAfter){
		if(pNewRange->pLast < pAfter->pFirst){
			break;
		}
		pAfter = (rangeItem *) pAfter->node.next;
	}

	if(pAfter){
		pBefore = (rangeItem *) pAfter->node.previous;
		lstInsert(pRangeList, &pBefore->node, &pNewRange->node);
	}
	else{
		lstAdd(pRangeList, &pNewRange->node);
	}
	FASTUNLOCK(&addrListLock);

	return SUCCESS;
}



/*
 *
 *      devAllocAddress()
 *
 *
 */
long    devAllocAddress(
const char		*pOwnerName,
epicsAddressType        addrType,
unsigned long		size,
unsigned		alignment, /* n ls bits zero in base addr*/
void                    **pLocalAddress)
{
	int		s;
	rangeItem	*pRange;
	char		*pBase;

	s = addrVerify(addrType, (void *)size);
	if(s){
		return s;
	}

	if(size == 0){
		return S_dev_lowValue;
	}

	FASTLOCK(&addrListLock);
        pRange = (rangeItem *) addrFree[addrType].node.next;
	while(pRange){
		if(pRange->pLast-pRange->pFirst>=size-1){
			s = blockFind(
				addrType,
				pRange->pFirst,
				pRange->pLast,
				size,
				alignment,
				&pBase);
			if(!s){
				break;
			}
		}
		pRange = (rangeItem *) pRange->node.next;
	}
	FASTUNLOCK(&addrListLock);

	if(!pRange){
		s = S_dev_deviceDoesNotFit;
		errMessage(s, epicsAddressTypeName[addrType]);
		return s;
	}


	s = devInstallAddr(
			pRange,
			pOwnerName,
			addrType,
			pBase,
			pBase + size - 1,
			pLocalAddress);
	return s;
}



/*
 *	addrVerify()
 */
LOCAL long    addrVerify(
epicsAddressType        addrType,
void                    *address)
{
	if(addrType>=atLast){
		return S_dev_uknAddrType;
	}

	if(address > addrLast[addrType]){
		return addrFail[addrType];
	}

	return SUCCESS;
}



/*
 * 	initAddrList()
 */
LOCAL long initAddrList(void)
{
        rangeItem       *pRange;

	if(NELEMENTS(addrAlloc) != NELEMENTS(addrFree)){
		return S_dev_internal;
	}

	if(!addrListInit){
		int i;

		FASTLOCKINIT(&addrListLock);

		FASTLOCK(&addrListLock);
		for(i=0; i<NELEMENTS(addrAlloc); i++){
			lstInit(&addrAlloc[i]);
			lstInit(&addrFree[i]);
		}

		addrListInit = TRUE;

		for(i=0; i<NELEMENTS(addrAlloc); i++){
			pRange = (rangeItem *)malloc(sizeof *pRange);
			if(!pRange){
				return S_dev_noMemory;
			}
			pRange->pOwnerName = "<Vacant>";
			pRange->pFirst = 0;
			pRange->pLast = addrLast[i];
			lstAdd(&addrFree[i], &pRange->node);
		}
		FASTUNLOCK(&addrListLock);
	}

	return SUCCESS;
}


/*
 * 	devAddressMap()
 */
long devAddressMap(void)
{
	return devListAddressMap(addrAlloc);
}


/*
 * 	devListAddressMap()
 */
LOCAL long devListAddressMap(LIST *pRangeList)
{
	rangeItem	*pri;
	int 		i;
	long		s;

	if(!addrListInit){
		s = initAddrList();
		if(s){
			return s;
		}
	}

	FASTLOCK(&addrListLock);
	for(i=0; i<NELEMENTS(addrAlloc); i++){
		pri = (rangeItem *) lstFirst(&pRangeList[i]);
		if(pri){
			printf("%s Address Map\n", epicsAddressTypeName[i]);
		}
		while(pri){
			printf("0X %0*lX - %0*lX %s\n",
				(int) (sizeof (pri->pFirst) * 2U),
				(unsigned long) pri->pFirst,
				(int) (sizeof (pri->pFirst) * 2U),
				(unsigned long) pri->pLast,
				pri->pOwnerName);
			pri = (rangeItem *) lstNext(&pri->node);
		}
	}
	FASTUNLOCK(&addrListLock);

	return SUCCESS;
}


/*
 *
 *	unsolicitedHandlerEPICS()
 *     	what gets called if they disconnect from an
 *	interrupt and an interrupt arrives on the
 *	disconnected vector
 *
 */
void 	unsolicitedHandlerEPICS(int vectorNumber)
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
        int     	i;
        SYM_TYPE	type;
        int     	status;
 
        for(i=0; i<NELEMENTS(defaultHandlerNames); i++){
                status =
                        symFindByName(  sysSymTbl,
                                        defaultHandlerNames[i],
                                        (char **)&defaultHandlerAddr[i],
                                        &type);
                if(status != OK){
			errPrintf(
				S_dev_internal,
				__FILE__,
				__LINE__,
				"initHandlerAddrList() %s not in sym table",
				defaultHandlerNames[i]);
		}
        }
}


/*
 *
 *      isrFetch()
 *
 *
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
 *
 *	vectorInUse()
 *
 *
 */
LOCAL int
vectorInUse(
unsigned       vectorNumber
)
{
	static int	init;
        int     	i;
        myISR		*psub;

	if(!init){
		initHandlerAddrList();
		init = TRUE;
	}
 
	psub = isrFetch(vectorNumber);

	/*
	 * its a C routine. Does it match a default handler?
	 */
        for(i=0; i<NELEMENTS(defaultHandlerAddr); i++){
                if(defaultHandlerAddr[i] == psub){
			return FALSE;
		}
	}

        return TRUE;
}




/*
 *
 * blockFind()
 *
 * Find unoccupied block in a large block
 *
 */
LOCAL long blockFind(
epicsAddressType	addrType,
char			*pBlockFirst,
char			*pBlockLast,
unsigned long		size,		/* size needed */
unsigned 		alignment,	/* n ls bits zero in base addr */
char			**ppBase	/* base address found */
)
{
	int		s;
	unsigned long	mask;

	/*
	 * align the block base
	 */
	mask = devCreateMask(alignment);
	if(mask&(long)pBlockFirst){
		pBlockFirst = (char *) (mask | (unsigned long) pBlockFirst);
		pBlockFirst++;
	}

	if(pBlockFirst == 0){
		return S_dev_badRequest;
	}

	if(pBlockLast < pBlockFirst){
		return S_dev_badRequest;
	}

	/*
	 * align size of block
	 */
	if(mask & size){ 
		size |= mask;
		size++;
	}

	if(size == 0){
		return S_dev_badRequest;
	}

	if(pBlockLast-pBlockFirst < size-1){
		return S_dev_badRequest;
	}

	s = blockDivide(
		addrType,
		pBlockFirst,
		pBlockFirst + (size-1),
		ppBase,
		size);

	return s;
}


/*
 * blockDivide()
 * (binary search within a block)
 * End up at a valid block without stepping through
 * the entire address range.
 */
LOCAL long blockDivide(
epicsAddressType	addrType,
char			*pBlockFirst,
char			*pBlockLast,
char			**ppBase,	/* base address found */
unsigned long		requestSize
)
{
	char		*pBlock;
	unsigned long	bs;
	int		s;

	s = blockProbe(addrType, pBlockFirst, pBlockFirst+(requestSize-1));
	if(!s){
		*ppBase = pBlockFirst;
		return SUCCESS;
	}

	/*
	 * prevent unsigned long overflow
	 */
	bs = pBlockLast - pBlockFirst + 1;
	if(bs == 0){
		bs = 0x80000000;
	}	
	else{
		bs = bs >> 1;
	}

	while(bs > requestSize){
		pBlock = pBlockFirst;
		while(pBlock <= pBlockLast-bs+1){
			pBlock += bs;
			s = blockProbe(addrType, pBlock, pBlock+(requestSize-1));
			if(!s){
				*ppBase = pBlock;
				return SUCCESS;
			}
			pBlock += bs;
		}
		bs = bs>>1;
	}
	return S_dev_deviceDoesNotFit;
}


/*
 * blockProbe()
 */
LOCAL long blockProbe(
epicsAddressType	addrType,
char			*pFirst,
char			*pLast
)
{
	char	*pProbe;
	int	s;

	pProbe = pFirst;
	while(pProbe <= pLast){
		s = locationProbe(addrType, pProbe);
		if(s){
			return s;
		}
		pProbe++;
	}
	return SUCCESS;
}


/*
 * locationProbe
 */
long locationProbe(
epicsAddressType	addrType,
char			*pLocation
)
{
	char	*pPhysical;
	int	s;

	/*
	 * every byte in the block must 
	 * map to a physical address
	 */
	s = sysBusToLocalAdrs(
             	        EPICStovxWorksAddrType[addrType],
			pLocation,
                        &pPhysical);
	if(s<0){
		return S_dev_vxWorksAddrMapFail;
	}


	{
		int8_t	*pChar;
		int8_t	byte;

		pChar = (int8_t *) pPhysical;
		if(devPtrAlignTest(pChar)){
			s = vxMemProbe(
				(char *) pChar,
				READ,
				sizeof(byte),
				(char *) &byte);
			if(s!=ERROR){
				return S_dev_addressOverlap;
			}			
		}
	}
	{
		int16_t	*pWord;
		int16_t	word;

		pWord = (int16_t *)pPhysical;
		if(devPtrAlignTest(pWord)){
			s = vxMemProbe(
				(char *)pWord,
				READ,
				sizeof(word),
				(char *) &word);
			if(s!=ERROR){
				return S_dev_addressOverlap;
			}			
		}
	}
	{
		int32_t	*pLongWord;
		int32_t	longWord;

		pLongWord = (int32_t *) pPhysical;
		if(devPtrAlignTest(pLongWord)){
			s = vxMemProbe(
				(char *)pLongWord,
				READ,
				sizeof(longWord),
				(char *)&longWord);
			if(s!=ERROR){
				return S_dev_addressOverlap;
			}			
		}
	}

	return SUCCESS;
}

/******************************************************************************
 *
 * The follwing may, or may not be present in the BSP for the CPU in use.
 *
 */
void *sysA24Malloc(unsigned long size);
STATUS sysA24Free(void *pBlock);

/******************************************************************************
 *
 * Routines to use to allocate and free memory present in the A24 region.
 *
 ******************************************************************************/

static void * (*A24MallocFunc)(size_t) = NULL;
static void   (*A24FreeFunc)(void *)     = NULL;
int devLibA24Debug = 0;		/* Debugging flag */

void *devLibA24Calloc(size_t size)
{
  void *ret;

  ret = devLibA24Malloc(size);

  if (ret == NULL)
    return (NULL);

  memset(ret, 0x00, size);
  return(ret);
}

void *devLibA24Malloc(size_t size)
{
  SYM_TYPE stype;
  static int    UsingBSP = 0;
  void		*ret;

  if (devLibA24Debug)
    logMsg("devLibA24Malloc(%d) entered\n", size, 0,0,0,0,0);

  if (A24MallocFunc == NULL)
  {
    /* See if the sysA24Malloc() function is present. */
    if(symFindByName(sysSymTbl,"_sysA24Malloc", (char**)&A24MallocFunc,&stype)==ERROR)
    { /* Could not find sysA24Malloc... use the malloc one and hope we are OK */
      if (devLibA24Debug)
	logMsg("devLibA24Malloc() using regular malloc\n",0,0,0,0,0,0);
      A24MallocFunc = malloc;
      A24FreeFunc   = free;
    }
    else
    {
      if(symFindByName(sysSymTbl,"_sysA24Free", (char**)&A24FreeFunc, &stype) == ERROR)
      { /* That's strange... we have malloc, but no free! */
        if (devLibA24Debug)
	  logMsg("devLibA24Malloc() using regular malloc\n",0,0,0,0,0,0);
        A24MallocFunc = malloc;
        A24FreeFunc   = free;
      }
      else
	UsingBSP = 1;
    }
  }
  ret = A24MallocFunc(size);

  if ((ret == NULL) && (UsingBSP))
    errMessage(S_dev_noMemory, "devLibA24Malloc ran out of A24 memory, try sysA24MapRam(size)");

  return(ret);
}

void devLibA24Free(void *pBlock)
{
  if (devLibA24Debug)
    logMsg("devLibA24Free(%p) entered\n", (unsigned long)pBlock,0,0,0,0,0);

  A24FreeFunc(pBlock);
}
