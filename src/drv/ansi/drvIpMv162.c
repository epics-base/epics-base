/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    drvIpMv162.c

Description:
    IPAC Carrier Driver for the IndustryPack carriers on the Motorola 
    MVME162 CPU board, provides the interface between IPAC driver and the 
    hardware.

Author:
    Andrew Johnson
Created:
    6 July 1995

(c) 1995 Royal Greenwich Observatory

*******************************************************************************/

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <vme.h>
#include <sysLib.h>
#include <vxLib.h>
#include "drvIpac.h"
#include "ipic.h"


/* Characteristics of the card */

#define SLOTS 4
#define IO_SPACES 2	/* Address spaces in A16 */
#define IPAC_IRQS 2	/* Interrupts per module */
#define IPIC_BASE 0xfffbc000


/* Base Addresses of IO and ID spaces */

#define REGS_A 0xfff58000
#define PROM_A 0xfff58080
#define REGS_B 0xfff58100
#define PROM_B 0xfff58180
#define REGS_C 0xfff58200
#define PROM_C 0xfff58280
#define REGS_D 0xfff58300
#define PROM_D 0xfff58380
#define REGS_AB 0xfff58400
#define REGS_CD 0xfff58500


/* IPIC chip */

ipic_t *ipic = (ipic_t *) IPIC_BASE;


/* IP Recovery Timers */

LOCAL const uchar_t recoveryTime[] = {
    IPIC_GEN_RT_0, 
    IPIC_GEN_RT_2, 
    IPIC_GEN_RT_2, 
    IPIC_GEN_RT_4, 
    IPIC_GEN_RT_4, 
    IPIC_GEN_RT_8, 
    IPIC_GEN_RT_8, 
    IPIC_GEN_RT_8, 
    IPIC_GEN_RT_8 
};


/* Carrier Base Address structure, only one instance can exist! */

LOCAL long mBase[IPAC_ADDR_SPACES][SLOTS] = {
    PROM_A,  PROM_B, PROM_C,  PROM_D,
    REGS_A,  REGS_B, REGS_C,  REGS_D,
    REGS_AB, NULL,   REGS_CD, NULL,
    NULL,    NULL,   NULL,    NULL
};


/*******************************************************************************

Routine:
    initialise

Purpose:
    Initialises the MVME162 IPIC chip with settings given in cardParams

Description:
    

Parameters:
    

Examples:
    "A:m=0x80000000,1024 l=4;B:l=2,2;C:m=0x80100000,64"

Returns:
    0 = OK, 
    S_IPAC_tooMany = Carrier already registered
    S_IPAC_badDriver = IPIC chip not found
    S_IPAC_badAddress = Parameter string error, or address not reachable

*/

LOCAL int initialise (
    char *cardParams,
    void **pprivate
) {
    static int initialised = FALSE;
    ushort_t slot;
    int count, p1, p2, next;
    char dummy, cmd;

    if (initialised) {
	return S_IPAC_tooMany;
    }

    if (vxMemProbe((void *)&ipic->chipId, READ, 1, &dummy) || 
	ipic->chipId != IPIC_CHIP_ID) {
	return S_IPAC_badDriver;
    }

    /* Initialise the IPIC chip */
    for (slot = 0; slot < SLOTS; slot++) {
	ipic->intCtrl[slot][0] = IPIC_INT_ICLR;
	ipic->intCtrl[slot][1] = IPIC_INT_ICLR;
	ipic->genCtrl[slot] = IPIC_GEN_WIDTH_16 | IPIC_GEN_RT_0;
    }

    /* Parse the parameter string */
    slot = 0;
    while ((cmd = *cardParams++) != '\0') {
	switch (cmd) {
	    case 'A':
	    case 'B':
	    case 'C':
	    case 'D':
		slot = cmd - 'A';
		break;

	    case 'm':
		p1 = p2 = 0;
		count = sscanf(cardParams, "=%p,%d%n", 
				(void **) &p1, &p2, &next);
		if (count != 2 ||
		    p1 < (long) sysMemTop() ||
		    p1 & 0xffff != 0 ||
		    p2 < 64 || p2 > 16384 ||
		    p1 + (p2*1024) > 0xfff00000) {
		    return S_IPAC_badAddress;
		}

		ipic->memBase[slot] = p1 >> 16;
		ipic->memSize[slot] = (p2 / 64) - 1;
		ipic->genCtrl[slot] |= IPIC_GEN_MEN;
		mBase[ipac_addrMem][slot] = p1;
		cardParams += next;
		break;

	    case 'l':
		p1 = p2 = 0;
		count = sscanf(cardParams, "=%d%n,%d%n", &p1, &next, &p2, &next);
		if (count < 1 || count > 2 ||
		    p1 < 0 || p1 > 7 ||
		    p2 < 0 || p2 > 7) {
		    return S_IPAC_badAddress;
		}

		ipic->intCtrl[slot][0] = (p1 & IPIC_INT_LEVEL) |
		    (ipic->intCtrl[slot][0] & ~IPIC_INT_LEVEL);
		ipic->intCtrl[slot][1] = (p2 & IPIC_INT_LEVEL) |
		    (ipic->intCtrl[slot][1] & ~IPIC_INT_LEVEL);
		cardParams += next;
		break;

	    case 'r':
		p1 = 0;
		count = sscanf(cardParams, "=%d%n", &p1, &next);
		if (count != 1 ||
		    p1 < 0 || p1 > 8) {
		    return S_IPAC_badAddress;
		}

		ipic->genCtrl[slot] = (ipic->genCtrl[slot] & ~IPIC_GEN_RT) | 
				      recoveryTime[p1];
		cardParams += next;
		break;
	    
	    case 'w':
		p1 = 0;
		count = sscanf(cardParams, "=%d%n", &p1, &next);
		if (count != 1) {
		    return S_IPAC_badAddress;
		}

		switch (p1) {
		    case 8:
			ipic->genCtrl[slot] = IPIC_GEN_WIDTH_8 |
				(ipic->genCtrl[slot] & ~IPIC_GEN_WIDTH);
			break;
		    case 16:
			ipic->genCtrl[slot] = IPIC_GEN_WIDTH_16 |
				(ipic->genCtrl[slot] & ~IPIC_GEN_WIDTH);
			break;
		    case 32:
			if (slot & 1) {
			    /* Illegal for odd-numbered slots */
			    return S_IPAC_badAddress;
			}
			ipic->genCtrl[slot] = IPIC_GEN_WIDTH_32 | 
				(ipic->genCtrl[slot] & ~IPIC_GEN_WIDTH);
			ipic->genCtrl[slot+1] &= ~(IPIC_GEN_WIDTH | 
						   IPIC_GEN_MEN);
			break;
		    default:
			return S_IPAC_badAddress;
		}
	}
    }

    initialised = TRUE;
    return OK;
}


/*******************************************************************************

Routine:
    baseAddr

Purpose:
    Returns the base address for the requested slot & address space

Description:
    This routine only has to do a table lookup in the mBase array.
    Note that no parameter checking is required - the IPAC driver which 
    calls this routine handles that.

Returns:
    The requested address, or NULL if the slot has no memory in the
    requested address space.

*/

LOCAL void *baseAddr (
    void *private,
    ushort_t slot,
    ipac_addr_t space
) {
    return (void *) mBase[space][slot];
}


/*******************************************************************************

Routine:
    irqCmd

Purpose:
    Handles interrupter commands and status requests

Description:
    The IPIC chip allows a lot of control over the IP interrupters, thus
    all commands perform the requested action.

Returns:
    ipac_irqGetLevel returns the current interrupt level,
    ipac_irqPoll returns >0 if interrupt line active else 0,
    other calls return 0 = OK.

*/

LOCAL int irqCmd (
    void *private,
    ushort_t slot,
    ushort_t irqNumber,
    ipac_irqCmd_t cmd
) {
    switch (cmd) {
	case ipac_irqLevel0:
	case ipac_irqLevel1:
	case ipac_irqLevel2:
	case ipac_irqLevel3:
	case ipac_irqLevel4:
	case ipac_irqLevel5:
	case ipac_irqLevel6:
	case ipac_irqLevel7:
	    ipic->intCtrl[slot][irqNumber] = (cmd & IPIC_INT_LEVEL) | 
		(ipic->intCtrl[slot][irqNumber] & ~IPIC_INT_LEVEL);
	    return OK;
	    
	case ipac_irqGetLevel:
	    return ipic->intCtrl[slot][irqNumber] & IPIC_INT_LEVEL;

	case ipac_irqEnable:
	    ipic->intCtrl[slot][irqNumber] |= IPIC_INT_IEN;
	    return OK;

	case ipac_irqDisable:
	    ipic->intCtrl[slot][irqNumber] &= ~IPIC_INT_IEN;
	    return OK;

	case ipac_irqPoll:
	    return ipic->intCtrl[slot][irqNumber] & IPIC_INT_INT;

	case ipac_irqSetEdge:
	    ipic->intCtrl[slot][irqNumber] |= IPIC_INT_EDGE;
	    return OK;

	case ipac_irqSetLevel:
	    ipic->intCtrl[slot][irqNumber] &= ~IPIC_INT_EDGE;
	    return OK;

	case ipac_irqClear:
	    ipic->intCtrl[slot][irqNumber] |= IPIC_INT_ICLR;
	    return OK;

	default:
	    return S_IPAC_notImplemented;
    }
}

/******************************************************************************/


/* IPAC Carrier Table */

ipac_carrier_t ipmv162 = {
    "Motorola MVME162",
    SLOTS,
    initialise,
    NULL,
    baseAddr,
    irqCmd
};

