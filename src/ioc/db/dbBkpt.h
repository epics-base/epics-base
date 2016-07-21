/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbBkpt.h			*/
/*
 *      Author:          Matthew Needes
 *      Date:            8-30-93
 */

#ifndef INCdbBkptsh
#define INCdbBkptsh 1

#include "ellLib.h"
#include "epicsEvent.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "shareLib.h"
#ifdef __cplusplus
extern "C" {
#endif

/*
 *  Structure containing a list of set breakpoints
 *    in a lockset
 */

struct BP_LIST {
   ELLNODE *next_list;
   ELLNODE *prev_list; 
   struct dbCommon *precord;
};

/*
 *  Structure containing queue of entrypoints
 *    detected for a lockset.
 */
struct EP_LIST {
   ELLNODE *next_list; 
   ELLNODE *prev_list;
   struct dbCommon *entrypoint;  /* pointer to entry point in lockset */
   unsigned long count;          /* number of times record processed */
   epicsTimeStamp time;           /* time record first logged */
   char sched;                   /* schedule record for next dbContTask() pass */
};

/*
 *  Structure for stack of lock sets that
 *  currently contain breakpoints. (uses ellLib)
 */
struct LS_LIST {
   ELLNODE *next_list;
   ELLNODE *prev_list;
   struct dbCommon *precord;/* points to where execution is currently stopped */
   struct dbCommon *current_ep; /* current entrypoint */
   ELLLIST bp_list;  /* list of records containing breakpoints in a lockset */
   ELLLIST ep_queue; /* queue of entrypoints found so far */
   epicsEventId ex_sem; /* semaphore for execution queue */
   epicsThreadId taskid;    /* saved taskid for the task in stepping mode */
   int step;      /* one if currently "stepping," else zero */
   unsigned long l_num;   /* lockset number */
};

/* Values for BKPT (breakpoint) field in record    */

/*  1st bit = 0 if breakpoint is not set,          */
/*            1 if breakpoint set                  */
/*  2nd bit = 0 if no printing after processing    */
/*            1 if print after processing set      */

/*  Breakpoint Masks */
#define BKPT_ON_MASK         0x001
#define BKPT_OFF_MASK        0x0FE
#define BKPT_PRINT_MASK      0x002
#define BKPT_PRINT_OFF_MASK  0x0FD

#define MAX_EP_COUNT         99999

epicsShareFunc void dbBkptInit(void);
epicsShareFunc long dbb(const char *recordname);
epicsShareFunc long dbd(const char *recordname);
epicsShareFunc long dbc(const char *recordname);
epicsShareFunc long dbs(const char *recordname);
epicsShareFunc long dbstat(void);
epicsShareFunc long dbp(
    const char *record_name, int interest_level);
epicsShareFunc long dbap(const char *record_name);
epicsShareFunc int  dbBkpt(struct dbCommon *precord);
epicsShareFunc void dbPrint(struct dbCommon *precord);
epicsShareFunc long dbprc(char *record_name);

extern long lset_stack_count;

#ifdef __cplusplus
}
#endif

#endif
