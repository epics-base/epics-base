/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    drvIpac.h

Description:
    IPAC Driver header file, defines the software interfaces:
	1. Upwards to the IPAC Module driver
	2. Downwards to the IPAC Carrier driver

Author:
    Andrew Johnson
Created:
    1 July 1995

(c) 1995 Royal Greenwich Observatory

*******************************************************************************/


#ifndef INCdrvIpacH
#define INCdrvIpacH

#include <types.h>
#include "ipModules.h"

#ifndef NO_EPICS
#include <errMdef.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* Error numbers */

#ifndef OK
#define OK 0
#endif

#ifndef M_ipac
#define M_ipac		(600 << 16)
#endif

#define S_IPAC_badTable   (M_ipac| 1) /*IPAC Carrier Table invalid*/
#define S_IPAC_tooMany    (M_ipac| 2) /*Too many IPAC carriers, table full*/
#define S_IPAC_badAddress (M_ipac| 3) /*Bad IPAC carrier or slot number*/
#define S_IPAC_badDriver  (M_ipac| 4) /*Bad value from IPAC carrier driver*/
#define S_IPAC_noModule   (M_ipac| 5) /*No IP module installed*/
#define S_IPAC_noIpacId   (M_ipac| 6) /*IPAC identifier not found*/
#define S_IPAC_badCRC     (M_ipac| 7) /*IPAC CRC Check failed*/
#define S_IPAC_badModule  (M_ipac| 8) /*IPAC Manufacturer or model ID wrong*/
#define S_IPAC_notImplemented (M_ipac| 9) /*IPAC Driver command not available*/


/* Structure of the IPAC ID Prom, located in the pack ID space. */

typedef struct {
    uchar_t pad00;
    uchar_t asciiI;
    uchar_t pad02;
    uchar_t asciiP;
    uchar_t pad04;
    uchar_t asciiA;
    uchar_t pad06;
    uchar_t asciiC;
    uchar_t pad08;
    uchar_t manufacturerId;
    uchar_t pad0a;
    uchar_t modelId;
    uchar_t pad0c;
    uchar_t revision;
    uchar_t pad0e;
    uchar_t reserved;
    uchar_t pad10;
    uchar_t driverIdLow;
    uchar_t pad12;
    uchar_t driverIdHigh;
    uchar_t pad14;
    uchar_t bytesUsed;
    uchar_t pad16;
    uchar_t CRC;
    uchar_t pad18;
    uchar_t packSpecific[0x3f-0x18];
} ipac_idProm_t;


/* These are the types of address space implemented in the IP
   specification.  Some IP modules only use the ID and IO spaces. */

#define IPAC_ADDR_SPACES 4

typedef enum {
    ipac_addrID = 0,	/* ID Prom space */
    ipac_addrIO = 1,	/* Registers etc */
    ipac_addrIO32 = 2,	/* Registers for 32-bit dual-slot */
    ipac_addrMem = 3	/* Memory space */
} ipac_addr_t;


/* The following are the possible commands to the carrier driver to 
   handle interrupts from the IP modules.  Some carriers will only be 
   able to implement a subset of these commands.  Note that irqEnable 
   should call the vxWorks sysBusEnable routine if this is needed to 
   pass the carrier interrupts through to the CPU. */

typedef enum {
    ipac_irqLevel0 = 0,	/* Disables interrupts */
    ipac_irqLevel1 = 1,	/* Lowest priority */
    ipac_irqLevel2 = 2,	
    ipac_irqLevel3 = 3,	
    ipac_irqLevel4 = 4,	
    ipac_irqLevel5 = 5,	
    ipac_irqLevel6 = 6,	/* Highest priority */
    ipac_irqLevel7 = 7,	/* Non-maskable, don't use */
    ipac_irqGetLevel,	/* Returns level set (or hard-coded) */
    ipac_irqEnable,	/* Required to use interrupts */
    ipac_irqDisable,	/* Not necessarily supported */
    ipac_irqPoll,	/* Returns interrupt state */
    ipac_irqSetEdge,	/* Sets edge-triggered interrupts */
    ipac_irqSetLevel,	/* Sets level-triggered (default) */
    ipac_irqClear	/* Only needed if using edge-triggered */
} ipac_irqCmd_t;


/* This is a table which each IPAC carrier driver provides to allow
   it to be queried by the IPAC driver.  One table is required for
   each type of carrier.  The cPrivate pointer is returned by the
   carrier driver initialise routine, and passed to all of the other
   routines as a means of identification of the carrier board. */

typedef struct {
    char *carrierType;
			/* String containing carrier board type */
    ushort_t numberSlots;
			/* Number of IPAC devices this carrier can hold */
    int (*initialise)(char *cardParams, void **cPrivate);
			/* Initialise carrier and return *cPrivate */
    char *(*report)(void *cPrivate, ushort_t slot);
			/* Return string with giving status of this slot */
    void *(*baseAddr)(void *cPrivate, ushort_t slot, ipac_addr_t space);
			/* Return base addresses for this slot */
    int (*irqCmd)(void *cPrivate, ushort_t slot, 
                  ushort_t irqNumber, ipac_irqCmd_t cmd);
			/* interrupt manipulation */
} ipac_carrier_t;


/* Functions for startup and interactive use */

extern int ipacAddCarrier(ipac_carrier_t *pcarrier, char *cardParams);
extern int ipacReport(int interest);
extern int ipacInitialise(int after);


/* Functions for use in IPAC module drivers */

extern int ipmCheck(ushort_t carrier, ushort_t slot);
extern int ipmValidate(ushort_t carrier, ushort_t slot,
		       uchar_t manufacturerId, uchar_t modelId);
extern char *ipmReport(ushort_t carrier, ushort_t slot);
extern void *ipmBaseAddr(ushort_t carrier, ushort_t slot, ipac_addr_t space);
extern int ipmIrqCmd(ushort_t carrier, ushort_t slot, 
		     ushort_t irqNumber, ipac_irqCmd_t cmd);


#ifdef __cplusplus
}
#endif

#endif /* INCipacH */

