/*****************************************************************************
*
* Author:		John Winans
* Date:			95-06-05
*
* $Id$
*
* $Log$
*
******************************************************************************/

#include <vxWorks.h>
#include <semLib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <dbCommon.h>
#include <dbAccess.h>

#include "bdt.h"

#define STATIC static
#define MESSAGE_PREFIX	"BDT PV server:"

/*****************************************************************************
*
* These conversion finctions take care of one of the most insane parts
* of dealing with database access... having two different interfaces that
* have the same named enumerators in two seperate header files... that
* therefore can not be both included in the same file.
*
* This is so bad, I wanted to vomit when typing it in.
*
******************************************************************************/
STATIC int DbrOld2New(int Old)
{
	switch (Old)
	{
	case 0: return(DBR_STRING);
	case 1: return(DBR_SHORT);
	case 2: return(DBR_FLOAT);
	case 3: return(DBR_ENUM);
	case 4: return(DBR_CHAR);
	case 5: return(DBR_LONG);
	case 6: return(DBR_DOUBLE);
	default:
		return(-1);
	}
}

STATIC int DbrNew2Old(int New)
{
	switch (New)
	{
	case DBR_STRING: return(0);
	case DBR_CHAR: return(4);
	case DBR_UCHAR: return(4);
	case DBR_SHORT: return(1);
	case DBR_USHORT: return(1);
	case DBR_LONG: return(5);
	case DBR_ULONG: return(5);
	case DBR_FLOAT: return(2);
	case DBR_DOUBLE: return(6);
	case DBR_ENUM: return(3);
	default:
		return(-1);
	}
}
/*****************************************************************************
*
* Handle the receipt of an OK message.
*
* The OK message is received as a confirmation of the last operation.  It is
* not normally responded to.
*
*****************************************************************************/
STATIC int BDT_ServiceOk(BDT *Bdt)
{
	printf("%s got a Ok message\n", MESSAGE_PREFIX);
	return(0);
}
/*****************************************************************************
*
* Handle the receipt of a Connect message.
*
* The Connect is received when a new connection is first made.  
* Any arguments left in the message body have not yet been read.
*
*****************************************************************************/
STATIC int BDT_ServiceConnect(BDT *Bdt)
{
	unsigned char	Length;
	char			Buf[100];
	struct dbAddr	*pDbAddr;

	Buf[0] = '\0';
	if (Bdt->remaining_recv > 0)
	{
		BdtReceiveData(Bdt, &Length, 1);
		if (Length <= sizeof(Buf))
		{
			BdtReceiveData(Bdt, Buf, Length);
			Buf[Length] = '\0';
		}
		else
		{
			printf("%s Connect message argument list too long\n", MESSAGE_PREFIX);
			BDT_DiscardMessage(Bdt);
			return(-1);
		}

	}
#ifdef DEBUG_VERBOSE
	printf("%s got Connect >%s<\n", MESSAGE_PREFIX, Buf);
#endif
	/* Find the PV in the database */
	Bdt->pService = malloc(sizeof(struct dbAddr));
	pDbAddr = (struct dbAddr *)(Bdt->pService);
	if (dbNameToAddr(Buf, pDbAddr))
	{
		BdtSendHeader(Bdt, BDT_Error, 0);
		free(Bdt->pService);
	}
	else
	{
		if (pDbAddr->dbr_field_type != pDbAddr->field_type)
		{
			BdtSendHeader(Bdt, BDT_Error, 0);
			free(Bdt->pService);
		}
		else
			BdtSendHeader(Bdt, BDT_Ok, 0);
	}
	return(0);
}
/*****************************************************************************
*
* Handle the receipt of an Error message.
*
*****************************************************************************/
STATIC int BDT_ServiceError(BDT *Bdt)
{
	printf("%s got a Error message\n", MESSAGE_PREFIX);
	return(0);
}
/*****************************************************************************
*
* Handle the receipt of a Get message.
*
* The response to a Get message is either an Error or a Value:
*
* Value message body format:
*	SHORT 	EPICS data type enumerator (in old format)
*	LONG	Number of elements
*	CHAR[]	Value image 
*
*
*****************************************************************************/
STATIC int BDT_ServiceGet(BDT *Bdt)
{
	void			*Buf;
	struct dbAddr	*pDbAddr = (struct dbAddr *)(Bdt->pService);
	long			NumElements;
	long			Size;
	long			l;
	short			OldType;
	int				stat;

#ifdef DEBUG_VERBOSE
	printf("%s got a Get message\n", MESSAGE_PREFIX);

	printf("field type=%d, field size=%d, elements=%d\n", pDbAddr->field_type, pDbAddr->field_size, pDbAddr->no_elements);
#endif

	OldType = DbrNew2Old(pDbAddr->field_type);
	if (OldType < 0)
	{
		BdtSendHeader(Bdt, BDT_Error, 0);
		return(0);
	}
	/* Allocate a buffer to hold the response message data */
	Buf = malloc(pDbAddr->field_size * pDbAddr->no_elements);
	if (Buf == NULL)
	{
		printf("Can't allocate %d-byte buffer for get request to %s\n",
			pDbAddr->field_size * pDbAddr->no_elements, 
			pDbAddr->precord->name);
		BdtSendHeader(Bdt, BDT_Error, 0);
		return(0);
	}
	/* Get the response message data */
	NumElements = pDbAddr->no_elements;
	if (stat=dbGetField(pDbAddr, pDbAddr->field_type, Buf, 0, &NumElements, NULL))
	{
		BdtSendHeader(Bdt, BDT_Error, 0);
		return(0);
	}
#if 0
	/* Test hack to transfer HUGE buffers */
	NumElements = pDbAddr->no_elements;
#endif
	/* Send the response message */
	Size = NumElements * pDbAddr->field_size;
	BdtSendHeader(Bdt, BDT_Value, Size + sizeof(long) + sizeof(short));
	BdtSendData(Bdt, &OldType, sizeof(short));
	BdtSendData(Bdt, &NumElements, sizeof(long));
	if (Size)
		BdtSendData(Bdt, Buf, Size);
	free(Buf);
	return(0);
}
/*****************************************************************************
*
* Handle the receipt of a Put message.
*
* Put message body format:
*	SHORT 	EPICS data type enumerator
*	LONG	Number of elements
*	CHAR[]	Value image 
*
*****************************************************************************/
STATIC int BDT_ServicePut(BDT *Bdt)
{
	long	Size;
	void	*Buf;
	short	DbrType;
	long	NumElements;
	struct dbAddr	*pDbAddr = (struct dbAddr *)(Bdt->pService);

#ifdef DEBUG_VERBOSE
	printf("%s got a Put message\n", MESSAGE_PREFIX);
#endif
	if (BdtGetResidualRead(Bdt) < 6)
	{
		BDT_DiscardMessage(Bdt);
		BdtSendHeader(Bdt, BDT_Error, 0);
		return(0);
	}
	if (BdtGetResidualRead(Bdt) == 6)
	{ /* Do data contents, just toss it */
		BDT_DiscardMessage(Bdt);
		BdtSendHeader(Bdt, BDT_Ok, 0);
		return(0);
	}
	Buf = malloc(BdtGetResidualRead(Bdt) - 6);
	if (Buf == NULL)
	{
		printf("Can't allocate %d-byte buffer for put request to %s\n",
			BdtGetResidualRead(Bdt), pDbAddr->precord->name);
		BdtSendHeader(Bdt, BDT_Error, 0);
		return(0);
	}
	BdtReceiveData(Bdt, &DbrType, 2);
	BdtReceiveData(Bdt, &NumElements, 4);

#ifdef DEBUG_VERBOSE
	printf("record field type=%d, field size=%d, elements=%d\n", pDbAddr->field_type, pDbAddr->field_size, pDbAddr->no_elements);
	printf("message field type=%d, field size=%d, elements=%d total %d\n", DbrType, pDbAddr->field_type ,NumElements, BdtGetResidualRead(Bdt));
#endif

	BdtReceiveData(Bdt, Buf, BdtGetResidualRead(Bdt));
	DbrType = DbrOld2New(DbrType);

	if (DbrType < 0)
		BdtSendHeader(Bdt, BDT_Error, 0);
	else if (dbPutField(pDbAddr, DbrType, Buf, NumElements))
		BdtSendHeader(Bdt, BDT_Error, 0);
	else
		BdtSendHeader(Bdt, BDT_Ok, 0);

	free(Buf);
	return(0);
}
/*****************************************************************************
*
* Handle the receipt of a Close message.
*
*****************************************************************************/
STATIC int BDT_ServiceClose(BDT *Bdt)
{
	printf("%s got a Close message\n", MESSAGE_PREFIX);
	free(Bdt->pService);
	BdtSendHeader(Bdt, BDT_Ok, 0);
	return(1);
}
/*****************************************************************************
*
* Handle the receipt of a Monitor message.
*
* Not Supported.
*
*****************************************************************************/
STATIC int BDT_ServiceMonitor(BDT *Bdt)
{
	printf("%s got a Monitor message\n", MESSAGE_PREFIX);
	BdtSendHeader(Bdt, BDT_Error, 0);
	return(0);
}
/*****************************************************************************
*
* Handle the receipt of a Value message.
*
* Not Supported.
*
*****************************************************************************/
STATIC int BDT_ServiceValue(BDT *Bdt)
{
	printf("%s got a Value message\n", MESSAGE_PREFIX);
	return(0);
}
/*****************************************************************************
*
* Handle the receipt of a Delta message.
*
* Not Supported.
*
*****************************************************************************/
STATIC int BDT_ServiceDelta(BDT *Bdt)
{
	printf("%s got a Delta message\n", MESSAGE_PREFIX);
	BdtSendHeader(Bdt, BDT_Ok, 0);
	return(0);
}
/*****************************************************************************
*
* Handle the receipt of an Add message.
*
* Not Supported.
*
*****************************************************************************/
STATIC int BDT_ServiceAdd(BDT *Bdt)
{
	printf("%s got a Add message\n", MESSAGE_PREFIX);
	BdtSendHeader(Bdt, BDT_Error, 0);
	return(0);
}
/*****************************************************************************
*
* Handle the receipt of a Delete message.
*
* Not Supported.
*
*****************************************************************************/
STATIC int BDT_ServiceDelete(BDT *Bdt)
{
	printf("%s got a Delete message\n", MESSAGE_PREFIX);
	BdtSendHeader(Bdt, BDT_Error, 0);
	return(0);
}
/*****************************************************************************
*
* Handle the receipt of a Ping message.
*
*****************************************************************************/
STATIC int BDT_ServicePing(BDT *Bdt)
{
	printf("%s got a Ping message\n", MESSAGE_PREFIX);
	BdtSendHeader(Bdt, BDT_Ok, 0);
	return(0);
}

BdtHandlers BDT_ServiceHandler_pv = 
{
	BDT_ServiceOk,
	BDT_ServiceConnect,
	BDT_ServiceError,
	BDT_ServiceGet,
	BDT_ServicePut,
	BDT_ServiceClose,
	BDT_ServiceMonitor,
	BDT_ServiceValue,
	BDT_ServiceDelta,
	BDT_ServiceAdd,
	BDT_ServiceDelete,
	BDT_ServicePing
};
