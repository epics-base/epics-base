/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    drvTip810.h

Description:
    Header file for TEWS TIP810 CAN Bus driver.

Author:
    Andrew Johnson
Created:
    20 July 1995

(c) 1995 Royal Greenwich Observatory

*******************************************************************************/


#ifndef INCdrvTip810H
#define INCdrvTip810H

#include <types.h>
#include "canBus.h"


/* Error Numbers */

#ifndef M_t810
#define M_t810			(810<<16)
#endif

#define S_t810_duplicateDevice	(M_t810| 1) /*duplicate t810 device definition*/
#define S_t810_badBusRate 	(M_t810| 2) /*CANbus bit rate not supported*/
#define S_t810_badDevice	(M_t810| 3) /*device pointer is not for t810*/
#define S_t810_transmitterBusy	(M_t810| 4) /*transmit buffer unexpectedly busy*/


extern int t810Report(int page);
extern int t810Create(char *busName, ushort_t card, ushort_t slot, 
		      uint_t busRate);
extern int t810Shutdown(int starttype);
extern int t810Initialise(void);

#endif /* INCdrvTip810H */

