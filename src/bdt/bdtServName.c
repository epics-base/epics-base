/*****************************************************************************
*
* Author:		John Winans
* Date:			95-06-05
*
* $Id$
*
* $Log$
*
*****************************************************************************/

#include <selectLib.h>
#include <stdio.h>
#include <signal.h>

/*****************************************************************************
*
* IOC listener task blocks on accept calls waiting for binders.
*	If a bind arrives, a receiver task is spawned.
*
* IOC receiver task blocks on read calls waiting for transactions.
*	When a transaction arrives it is serviced.
*	At the end of a transaction service, a response is sent back.
*	After the response is sent, a chack is made to see if a delta transmission
*	was blocked by the transaction's use of the socket... if so, it is sent.
*
******************************************************************************/

#include <vxWorks.h>
#include <sys/socket.h>
#include <in.h>
#include <inetLib.h>
#include <taskLib.h>

#include "bdt.h"

#define	STATIC	static

/*****************************************************************************
*
*****************************************************************************/
STATIC int BDT_NameServiceOk(BDT *Bdt)
{
	printf("BDT_NameServiceOk \n");
	return(0);
}
/*****************************************************************************
*
*****************************************************************************/
STATIC int BDT_NameServiceConnect(BDT *Bdt)
{
	printf("BDT_NameServiceConnect \n");
	BdtSendHeader(Bdt, BDT_Ok, 0);
	return(0);
}
STATIC int BDT_NameServiceError(BDT *Bdt)
{
	printf("BDT_NameServiceError \n");
	return(0);
}
STATIC int BDT_NameServiceGet(BDT *Bdt)
{
	printf("BDT_NameServiceGet \n");
	BdtSendHeader(Bdt, BDT_Error, 0);
	return(0);
}
STATIC int BDT_NameServicePut(BDT *Bdt)
{
	printf("BDT_NameServicePut \n");
	BdtSendHeader(Bdt, BDT_Error, 0);
	return(0);
}
STATIC int BDT_NameServiceClose(BDT *Bdt)
{
	printf("BDT_NameServiceClose \n");
	BdtSendHeader(Bdt, BDT_Ok, 0);
	return(0);
}
STATIC int BDT_NameServiceMonitor(BDT *Bdt)
{
	printf("BDT_NameServiceMonitor \n");
	BdtSendHeader(Bdt, BDT_Error, 0);
	return(0);
}
STATIC int BDT_NameServiceValue(BDT *Bdt)
{
	printf("BDT_NameServiceValue \n");
	return(0);
}
STATIC int BDT_NameServiceDelta(BDT *Bdt)
{
	printf("BDT_NameServiceDelta \n");
	BdtSendHeader(Bdt, BDT_Error, 0);
	return(0);
}
STATIC int BDT_NameServiceAdd(BDT *Bdt)
{
	printf("BDT_NameServiceAdd \n");
	BdtSendHeader(Bdt, BDT_Error, 0);
	return(0);
}
STATIC int BDT_NameServiceDelete(BDT *Bdt)
{
	printf("BDT_NameServiceDelete \n");
	BdtSendHeader(Bdt, BDT_Error, 0);
	return(0);
}
STATIC int BDT_NameServicePing(BDT *Bdt)
{
	printf("BDT_NameServicePing \n");
	BdtSendHeader(Bdt, BDT_Ok, 0);
	return(0);
}

BdtHandlers BDT_ServiceHandler_name = 
{
	BDT_NameServiceOk,
	BDT_NameServiceConnect,
	BDT_NameServiceError,
	BDT_NameServiceGet,
	BDT_NameServicePut,
	BDT_NameServiceClose,
	BDT_NameServiceMonitor,
	BDT_NameServiceValue,
	BDT_NameServiceDelta,
	BDT_NameServiceAdd,
	BDT_NameServiceDelete,
	BDT_NameServicePing
};
