/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    canBus.h

Description:
    CANBUS specific constants

Author:
    Andrew Johnson
Created:
    25 July 1995

(c) 1995 Royal Greenwich Observatory

*******************************************************************************/


#ifndef INCcanBusH
#define INCcanBusH


#define CAN_IDENTIFIERS 2048
#define CAN_DATA_SIZE 8

#define CAN_BUS_OK 0
#define CAN_BUS_ERROR 1
#define CAN_BUS_OFF 2


#ifndef M_can
#define M_can			(811<<16)
#endif

#define S_can_badMessage	(M_can| 1) /*illegal CAN message contents*/
#define S_can_badAddress	(M_can| 2) /*CAN address syntax error*/
#define S_can_noDevice		(M_can| 3) /*CAN bus name does not exist*/

typedef struct {
    ushort_t identifier;	/* 0 .. 2047 with holes! */
    enum { 
	SEND = 0, RTR = 1
    } rtr;			/* Remote Transmission Request */
    uchar_t length;		/* 0 .. 8 */
    uchar_t data[CAN_DATA_SIZE];
} canMessage_t;

typedef struct {
    char *busName;
    int timeout;
    ushort_t identifier;
    ushort_t offset;
    signed int parameter;
    void *canBusID;
} canIo_t;

typedef void canMsgCallback_t(void *pprivate, canMessage_t *pmessage);
typedef void canSigCallback_t(void *pprivate, int status);

extern int canOpen(char *busName, void **pcanBusID);
extern int canRead(void *canBusID, canMessage_t *pmessage, int timeout);
extern int canWrite(void *canBusID, canMessage_t *pmessage, int timeout);
extern int canMessage(void *canBusID, ushort_t identifier, 
		      canMsgCallback_t callback, void *pprivate);
extern int canSignal(void *canBusID, canSigCallback_t callback, void *pprivate);
extern int canIoParse(char *canString, canIo_t *pcanIo);


#endif /* INCcanBusH */

