/***************************************************************************
 *   File:		epicsGeneralTime.c
 *   Author:		Sheng Peng
 *   Institution:	Oak Ridge National Laboratory / SNS Project
 *   Date:		07/2004
 *   Version:		1.3
 *
 *   EPICS general timestamp support
 *   integration into EPICS Base by Peter Denison, Diamond Light Source
 *
 * Copyright (c) 2008 Diamond Light Source Ltd
 * Copyright (c) 2004 Oak Ridge National Laboratory
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <epicsTypes.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsMessageQueue.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <epicsInterrupt.h>
#include <osiSock.h>
#include <ellLib.h>
#include <errlog.h>
#include <cantProceed.h>

#include <envDefs.h>

#include "generalTimeSup.h"
#include "epicsGeneralTime.h"
#define GENERALTIME_VERSION "GeneralTime Framework Version 1.2"

#define TP_DESC_LEN 40
#define NUM_OF_EVENTS   256

typedef struct TIME_CURRENT_PROVIDER {
    ELLNODE                 node;   /* Linked List Node */
    char                    tp_desc[TP_DESC_LEN];
    pepicsTimeGetCurrent    tcp_getCurrent;
    int                     tcp_priority;
} TIME_CURRENT_PROVIDER;

typedef struct TIME_EVENT_PROVIDER {
    ELLNODE                 node;   /* Linked List Node */
    char                    tp_desc[TP_DESC_LEN];
    pepicsTimeGetEvent      tep_getEvent;
    int                     tep_priority;
} TIME_EVENT_PROVIDER;

typedef struct  generalTimePvt
{
    epicsMutexId            tcp_list_sem;   /* This is a mutex semaphore to protect time-current-provider-list operation */
    ELLLIST                 tcp_list;   /* time current provider list */
    TIME_CURRENT_PROVIDER   *pLastKnownBestTcp;
    epicsTimeStamp          lastKnownTimeCurrent;

    epicsMutexId            tep_list_sem;   /* This is a mutex semaphore to protect time-event-provider-list operation */
    ELLLIST                 tep_list;   /* time event provider list */
    TIME_EVENT_PROVIDER     *pLastKnownBestTep;
    epicsTimeStamp          lastKnownTimeEvent[NUM_OF_EVENTS];
    epicsTimeStamp          lastKnownTimeEventBestTime;

    epicsTimerQueueId       sync_queue;
    unsigned int            ErrorCounts;
} generalTimePvt;

int	        generalTimeGetCurrent(epicsTimeStamp *pDest);
static int	generalTimeGetCurrentPriority(epicsTimeStamp *pDest, int * pPriority);
int         generalTimeGetEvent(epicsTimeStamp *pDest,int eventNumber);
static int	generalTimeGetEventPriority(epicsTimeStamp *pDest,int eventNumber, int * pPriority);

long        generalTimeReport(int level);

static  generalTimePvt *pgeneralTimePvt=NULL;
static  char logBuffer[160];

int     GENERALTIME_DEBUG=0;

/* implementation */
void    generalTime_InitOnce(void)
{
	pgeneralTimePvt = (generalTimePvt *)callocMustSucceed(1,sizeof(generalTimePvt),"generalTime_Init");

	ellInit(&(pgeneralTimePvt->tcp_list));
	pgeneralTimePvt->tcp_list_sem = epicsMutexMustCreate();

	ellInit(&(pgeneralTimePvt->tep_list));
	pgeneralTimePvt->tep_list_sem = epicsMutexMustCreate();

	/* Initialise the "last-resort" provider on a per-architecture basis */
	osdTimeInit();
}

void generalTime_Init(void)
{
    /* We must only initialise generalTime once */
    static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;

    epicsThreadOnce(&onceId, (EPICSTHREADFUNC)generalTime_InitOnce, NULL);
}

int     epicsTimeGetCurrent(epicsTimeStamp *pDest)
{
	int	priority;
	return(generalTimeGetCurrentPriority(pDest, &priority));
}

static int     generalTimeGetCurrentPriority(epicsTimeStamp *pDest, int * pPriority)
{
	return(generalTimeGetExceptPriority(pDest, pPriority, 0));	/* no tcp is excluded */

}

int	generalTimeGetExceptPriority(epicsTimeStamp *pDest, int * pPriority, int except_tcp)
{
	int key;
	TIME_CURRENT_PROVIDER   * ptcp;
	int			status = epicsTimeERROR;
	if(!pgeneralTimePvt)
		generalTime_Init();

	epicsMutexMustLock( pgeneralTimePvt->tcp_list_sem );
	for(ptcp=(TIME_CURRENT_PROVIDER *)ellFirst( &(pgeneralTimePvt->tcp_list) );
		ptcp; ptcp=(TIME_CURRENT_PROVIDER *)ellNext((ELLNODE *)ptcp))
	{/* when we register providers, we sort them by priority */
		/* Allow providers to ask for time not including their own */
		if (ptcp->tcp_priority == except_tcp) {
			continue;
		}
		if(ptcp->tcp_getCurrent)
			status=(*(ptcp->tcp_getCurrent))(pDest);
		if(status!=epicsTimeERROR)
		{/* we can check if time monotonic here */
			if (epicsTimeGreaterThanEqual(pDest, &(pgeneralTimePvt->lastKnownTimeCurrent))) {
				pgeneralTimePvt->lastKnownTimeCurrent=*pDest;
				pgeneralTimePvt->pLastKnownBestTcp=ptcp;
				*pPriority = ptcp->tcp_priority;
			} else {
				epicsTimeStamp orig = *pDest;
				*pDest=pgeneralTimePvt->lastKnownTimeCurrent;
				key = epicsInterruptLock();	/* OSI has no taskLock, but if we lock interrupt, it is good enough */
				pgeneralTimePvt->ErrorCounts++;
				epicsInterruptUnlock(key);	/* OSI has no taskLock, but if we lock interrupt, it is good enough */
				if(GENERALTIME_DEBUG) {
                    sprintf(logBuffer, "TCP provider \"%s\" provides backwards time!\nnew = %us, %uns, last = %us, %uns (%s)\n",
                            ptcp->tp_desc,orig.secPastEpoch,orig.nsec,pDest->secPastEpoch,pDest->nsec,
                            pgeneralTimePvt->pLastKnownBestTcp->tp_desc);
					epicsInterruptContextMessage(logBuffer);
				}
				*pPriority = pgeneralTimePvt->pLastKnownBestTcp->tcp_priority;
			}
			break;
		}
	}
	if(status==epicsTimeERROR)
		pgeneralTimePvt->pLastKnownBestTcp=NULL;
	epicsMutexUnlock( pgeneralTimePvt->tcp_list_sem );

	return	status;
}

int     epicsTimeGetEvent(epicsTimeStamp *pDest,int eventNumber)
{
	int	priority;
	if (eventNumber == epicsTimeEventCurrentTime) {
	    return generalTimeGetCurrentPriority(pDest, &priority);
	} else {
	    return(generalTimeGetEventPriority(pDest, eventNumber, &priority));
	}
}

static int     generalTimeGetEventPriority(epicsTimeStamp *pDest,int eventNumber, int * pPriority)
{
	int key;
	TIME_EVENT_PROVIDER   * ptep;
	int                     status = epicsTimeERROR;
	if(!pgeneralTimePvt)
		generalTime_Init();

	epicsMutexMustLock( pgeneralTimePvt->tep_list_sem );
	for(ptep=(TIME_EVENT_PROVIDER *)ellFirst( &(pgeneralTimePvt->tep_list) );
		ptep; ptep=(TIME_EVENT_PROVIDER *)ellNext((ELLNODE *)ptep))
	{/* when we register provider, we sort them by priority */
		if(ptep->tep_getEvent)
			status=(*(ptep->tep_getEvent))(pDest,eventNumber);
		if(status!=epicsTimeERROR)
		{/* we can check if time monotonic here */
			pgeneralTimePvt->pLastKnownBestTep=ptep;
			*pPriority = ptep->tep_priority;
			if(eventNumber>=0 && eventNumber<NUM_OF_EVENTS)
			{
				if (epicsTimeGreaterThanEqual(pDest,&(pgeneralTimePvt->lastKnownTimeEvent[eventNumber]))) {
					pgeneralTimePvt->lastKnownTimeEvent[eventNumber]=*pDest;
				} else {
					*pDest=pgeneralTimePvt->lastKnownTimeEvent[eventNumber];
					key = epicsInterruptLock();	/* OSI has no taskLock, but if we lock interrupt, it is good enough */
					pgeneralTimePvt->ErrorCounts++;
					epicsInterruptUnlock(key);	/* OSI has no taskLock, but if we lock interrupt, it is good enough */
					if(GENERALTIME_DEBUG) {
                        sprintf(logBuffer, "TEP provider \"%s\" provides backwards time on Event %d!\n", ptep->tp_desc, eventNumber);
						epicsInterruptContextMessage(logBuffer);
					}
				}
			}
			if(eventNumber==epicsTimeEventBestTime)
			{
				if (epicsTimeGreaterThanEqual(pDest,&(pgeneralTimePvt->lastKnownTimeEventBestTime))) {
					pgeneralTimePvt->lastKnownTimeEventBestTime=*pDest;
				} else {
					*pDest=pgeneralTimePvt->lastKnownTimeEventBestTime;
					key = epicsInterruptLock();	/* OSI has no taskLock, but if we lock interrupt, it is good enough */
					pgeneralTimePvt->ErrorCounts++;
					epicsInterruptUnlock(key);	/* OSI has no taskLock, but if we lock interrupt, it is good enough */
					if(GENERALTIME_DEBUG) {
                        sprintf(logBuffer, "TEP provider \"%s\" provides backwards time on Event BestTime!\n", ptep->tp_desc);
						epicsInterruptContextMessage(logBuffer);
					}
				}
			}
			break;
		}
	}
	if(status==epicsTimeERROR)
		pgeneralTimePvt->pLastKnownBestTep=NULL;
	epicsMutexUnlock( pgeneralTimePvt->tep_list_sem );

	return  status;
}

/*
 * For all the similarity of the following two functions, they are
 * walking separate lists, and assigning to structures of different
 * types, all because of the difference in function prototype between
 * the getCurrent and getEvent functions. While they could be templated,
 * I think it's simpler to leave the duplication there.
 */ 

int		generalTimeEventTpRegister(const char *desc, int priority, pepicsTimeGetEvent getEvent)
{
	TIME_EVENT_PROVIDER     * ptep, * ptepref;

	if(!pgeneralTimePvt)
		generalTime_Init();

	ptep=(TIME_EVENT_PROVIDER *)malloc(sizeof(TIME_EVENT_PROVIDER));

	if(ptep==NULL) {
		return	epicsTimeERROR;
	}

	strncpy(ptep->tp_desc,desc,TP_DESC_LEN-1);
	ptep->tp_desc[TP_DESC_LEN-1]='\0';
	ptep->tep_priority=priority;
	ptep->tep_getEvent=getEvent;

	epicsMutexMustLock( pgeneralTimePvt->tep_list_sem );
	for(ptepref=(TIME_EVENT_PROVIDER *)ellFirst( &(pgeneralTimePvt->tep_list) );
		ptepref; ptepref=(TIME_EVENT_PROVIDER *)ellNext((ELLNODE *)ptepref))
	{
		if(ptepref->tep_priority > ptep->tep_priority)  break;
	}
	if(ptepref)
	{/* found a ref whose priority is just bigger than the new one */
		ptepref=(TIME_EVENT_PROVIDER *)ellPrevious((ELLNODE *)ptepref);
		ellInsert( &(pgeneralTimePvt->tep_list), (ELLNODE *)ptepref, (ELLNODE *)ptep );
	}
	else
	{/* either list is empty or no one have bigger(lower) priority, put on tail */
		ellAdd( &(pgeneralTimePvt->tep_list), (ELLNODE *)ptep );
	}
	epicsMutexUnlock( pgeneralTimePvt->tep_list_sem );

	return	epicsTimeOK;
}

int		generalTimeCurrentTpRegister(const char *desc, int priority, pepicsTimeGetCurrent getCurrent)
{
	TIME_CURRENT_PROVIDER   * ptcp, * ptcpref;

	if(!pgeneralTimePvt)
		generalTime_Init();

	ptcp=(TIME_CURRENT_PROVIDER *)malloc(sizeof(TIME_CURRENT_PROVIDER));

	if(ptcp==NULL) {
		return	epicsTimeERROR;
	}

	strncpy(ptcp->tp_desc,desc,TP_DESC_LEN-1);
	ptcp->tp_desc[TP_DESC_LEN-1]='\0';
	ptcp->tcp_priority=priority;
	ptcp->tcp_getCurrent=getCurrent;

	epicsMutexMustLock( pgeneralTimePvt->tcp_list_sem );
	for(ptcpref=(TIME_CURRENT_PROVIDER *)ellFirst( &(pgeneralTimePvt->tcp_list) );
		ptcpref; ptcpref=(TIME_CURRENT_PROVIDER *)ellNext((ELLNODE *)ptcpref))
	{
		if(ptcpref->tcp_priority > ptcp->tcp_priority)	break;
	}
	if(ptcpref)
	{/* found a ref whose priority is just bigger than the new one */
		ptcpref=(TIME_CURRENT_PROVIDER *)ellPrevious((ELLNODE *)ptcpref);
		ellInsert( &(pgeneralTimePvt->tcp_list), (ELLNODE *)ptcpref, (ELLNODE *)ptcp );
	}
	else
	{/* either list is empty or no one have bigger(lower) priority, put on tail */
		ellAdd( &(pgeneralTimePvt->tcp_list), (ELLNODE *)ptcp );
	}
	epicsMutexUnlock( pgeneralTimePvt->tcp_list_sem);

	return	epicsTimeOK;
}

/* 
 * Provide an optional "last resort" provider for Event Time.
 * 
 * This is deliberately optional, as it represents site policy.
 * It is intended to be installed as an EventTime provider at the lowest
 * priority, to return the current time for an event if there is no
 * better time provider for event times.
 * 
 * Typically, this will only be used during startup, or a time-provider
 * resynchronisation, where events are being generated by the event system
 * but the time provided by the system is not yet valid.
 */
static int lastResortGetEvent(epicsTimeStamp *timeStamp, int eventNumber) 
{
	return epicsTimeGetCurrent(timeStamp);
}

int lastResortEventProviderInstall(void)
{
	return generalTimeEventTpRegister("Last Resort Event", LAST_RESORT_PRIORITY, lastResortGetEvent);
}

epicsTimerId generalTimeCreateSyncTimer(epicsTimerCallback cb, void* param)
{
	if (!pgeneralTimePvt->sync_queue) {
		pgeneralTimePvt->sync_queue = epicsTimerQueueAllocate(0, epicsThreadPriorityHigh);
	}
	return epicsTimerQueueCreateTimer(pgeneralTimePvt->sync_queue, cb, param);
}

long    generalTimeReport(int level)
{
	TIME_CURRENT_PROVIDER	* ptcp;
	TIME_EVENT_PROVIDER	* ptep;

	int			status;
	epicsTimeStamp		tempTS;
	char			tempTSText[40];

	int			items;		/* how many provider we have */
	char			* ptempText;	/* logMsg passed pointer instead of strcpy, so we have to keep a local screen copy then printf */
	int			tempTextOffset;

	printf(GENERALTIME_VERSION"\n");

	if(!pgeneralTimePvt)
	{/* GeneralTime is not used, we just report version then quit */
		printf("General time framework is not initialized yet!\n\n");
		return	epicsTimeOK;
	}

	/* GeneralTime is in use, we try to report more detail */

	/* we use sprintf instead of printf because we don't want to hold xxx_list_sem too long */

	if(level>0)
	{
		printf("\nFor Time-Current-Provider:\n");
		epicsMutexMustLock( pgeneralTimePvt->tcp_list_sem );
		if(( items = ellCount( &(pgeneralTimePvt->tcp_list)) ))
		{
			ptempText = (char *)malloc(items * 80 * 3);	/* for each provider, we print 3 lines, and each line is less then 80 bytes !!!!!!!! */
			if(!ptempText)
			{/* malloc failed */
				epicsMutexUnlock( pgeneralTimePvt->tcp_list_sem );
				printf("Malloced memory for print out for %d tcps failed!\n", items);
				return epicsTimeERROR;
			}
			if(GENERALTIME_DEBUG)       printf("Malloced memory for print out for %d tcps\n", items);

			bzero(ptempText, items*80*3);
			tempTextOffset = 0;

			for(ptcp=(TIME_CURRENT_PROVIDER *)ellFirst( &(pgeneralTimePvt->tcp_list) );
				ptcp; ptcp=(TIME_CURRENT_PROVIDER *)ellNext((ELLNODE *)ptcp))
			{
				tempTextOffset += sprintf(ptempText+tempTextOffset, "\t\"%s\",priority %d\n", ptcp->tp_desc, ptcp->tcp_priority);
				if(level>1)
				{
					tempTextOffset += sprintf( ptempText+tempTextOffset, "\t getCurrent is 0x%lx\n",
									(unsigned long)(ptcp->tcp_getCurrent) );
					if(ptcp->tcp_getCurrent)
					{
						status=(*(ptcp->tcp_getCurrent))(&tempTS);
						if(status!=epicsTimeERROR)
						{
							tempTSText[0]='\0';
							epicsTimeToStrftime(tempTSText, sizeof(tempTSText), "%Y/%m/%d %H:%M:%S.%06f",&tempTS);
							tempTextOffset += sprintf(ptempText+tempTextOffset, "\t Current Time is %s!\n", tempTSText);
						}
						else
						{
							tempTextOffset += sprintf(ptempText+tempTextOffset, "\t Time Current Provider \"%s\" Failed!\n", ptcp->tp_desc);
						}
					}
				}
			}
			epicsMutexUnlock( pgeneralTimePvt->tcp_list_sem );
			printf("%s", ptempText);
			free(ptempText);
		}
		else
		{
			epicsMutexUnlock( pgeneralTimePvt->tcp_list_sem );
			printf("\tNo Time-Current-Provider registered!\n");
		}

		printf("For Time-Event-Provider:\n");
		epicsMutexMustLock( pgeneralTimePvt->tep_list_sem );
		if(( items = ellCount( &(pgeneralTimePvt->tep_list) ) ))
		{
                        ptempText = (char *)malloc(items * 80 * 2);     /* for each provider, we print 2 lines, and each line is less then 80 bytes !!!!!!!! */
                        if(!ptempText)
                        {/* malloc failed */
                                epicsMutexUnlock( pgeneralTimePvt->tep_list_sem );
                                printf("Malloced memory for print out for %d teps failed!\n", items);
                                return epicsTimeERROR;
                        }
                        if(GENERALTIME_DEBUG)       printf("Malloced memory for print out for %d teps\n", items);

                        bzero(ptempText, items*80*2);
                        tempTextOffset = 0;

			for(ptep=(TIME_EVENT_PROVIDER *)ellFirst( &(pgeneralTimePvt->tep_list) );
				ptep; ptep=(TIME_EVENT_PROVIDER *)ellNext((ELLNODE *)ptep))
			{
				tempTextOffset += sprintf(ptempText+tempTextOffset,"\t\"%s\",priority %d\n",ptep->tp_desc,ptep->tep_priority);
				if(level>1)
					tempTextOffset += sprintf( ptempText+tempTextOffset, "\t getEvent is 0x%lx\n",(unsigned long)(ptep->tep_getEvent) );
			}
			epicsMutexUnlock( pgeneralTimePvt->tep_list_sem );
			printf("%s", ptempText);
			free(ptempText);
		}
		else
		{
			epicsMutexUnlock( pgeneralTimePvt->tep_list_sem );
			printf("\tNo Time-Event-Provider registered!\n");
		}

		printf("\n");
	}
	return	epicsTimeOK;
}

/*
 * The following functions are accessors for various internal values, so that
 * they can be made available to device support. They are used by the
 * devGeneralTime.c file in <base>/src/dev/softDev which implements the
 * 'generalTime' DTYP for ai, bo, longin and stringin records 
 */

int	generalTimeGetCurrentDouble(double * pseconds)	/* for ai record, seconds from 01/01/1990 */
{
	epicsTimeStamp          ts;
	if(epicsTimeERROR!=epicsTimeGetCurrent(&ts))
	{
		*pseconds=ts.secPastEpoch+((double)(ts.nsec))*1e-9;
		return	epicsTimeOK;
	}
	else
		return  epicsTimeERROR;
}

void    generalTimeResetErrorCounts()	/* for bo record */
{
	if(!pgeneralTimePvt)
		generalTime_Init();
	pgeneralTimePvt->ErrorCounts=0;
}

int     generalTimeGetErrorCounts()	/* for longin record */
{
	if(!pgeneralTimePvt)
		generalTime_Init();
	return	pgeneralTimePvt->ErrorCounts;
}

void    generalTimeGetBestTcp(char * desc)	/* for stringin record */
{/* the assignment to pLastKnownBestTcp is atomic and desc is never changed after registeration */
	if(!pgeneralTimePvt)
		generalTime_Init();
	if(pgeneralTimePvt->pLastKnownBestTcp)
	{/* We know some good tcp */
		strncpy(desc,pgeneralTimePvt->pLastKnownBestTcp->tp_desc,TP_DESC_LEN-1);
		desc[TP_DESC_LEN-1]='\0';
	}
	else
	{/* no good tcp */
		strncpy(desc,"No Good Time-Current-Provider",TP_DESC_LEN-1);
		desc[TP_DESC_LEN-1]='\0';
	}
}

void    generalTimeGetBestTep(char * desc)	/* for stringin record */
{/* the assignment to pLastKnownBestTep is atomic and desc is never changed after registeration */
	if(!pgeneralTimePvt)
		generalTime_Init();
	if(pgeneralTimePvt->pLastKnownBestTep)
	{/* We know some good tep */
		strncpy(desc,pgeneralTimePvt->pLastKnownBestTep->tp_desc,TP_DESC_LEN-1);
		desc[TP_DESC_LEN-1]='\0';
	}
	else
	{/* no good tep */
		strncpy(desc,"No Good Time-Event-Provider",TP_DESC_LEN-1);
		desc[TP_DESC_LEN-1]='\0';
	}
}
