/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbBkpt.c */
/*
 *      Author:          Matthew Needes
 *      Date:            8-30-93
*/

/*
 *   Database Breakpoint Manipulation and User Interface
 *
 * USER COMMANDS
 *  dbb(record_name)   Set a breakpoint in a record
 *  dbd(record_name)   Delete a record's breakpoint
 *  dbc(record_name)   Resume record processing
 *  dbs(record_name)   Step through record processing through
 *                        IO links, forward process links, etc.
 *  dbstat()           Display status of stopped records in lock sets.
 *  dbap(record_name)  Toggle automatic print after processing.
 *  dbp(record_name)   Print out fields from record currently stopped.
 *  dbprc(record_name) Processes a record once without printing it.
 *                          (Unless autoprint is on)
 *
 * INTERNAL FUNCTIONS
 *  dbBkpt()           Process breakpoints, called by dbProcess().
 *  dbPrint()          Prints record if autoprint enabled.
 *  dbBkptCont()       The task that continues and steps through
 *                         records that are stopped at a breakpoint.
 */

/* #define BKPT_DIAG */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "errlog.h"
#include "errMdef.h"

#define epicsExportSharedSymbols
#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbBkpt.h"
#include "dbCommon.h"
#include "db_field_log.h"
#include "dbFldTypes.h"
#include "dbFldTypes.h"
#include "dbLink.h"
#include "dbLock.h"
#include "dbScan.h"
#include "dbTest.h"
#include "link.h"
#include "recGbl.h"
#include "recSup.h"
#include "special.h"

/* private routines */
static void dbBkptCont(dbCommon *precord);
static long FIND_CONT_NODE(
  const char *record_name,
  struct LS_LIST **ppnode,
  struct dbCommon **pprecord);

/*
 *  Breakpoints are used as a debugging instrument to suspend the
 *    processing of database records.  Once suspended, record
 *    processing may continue if either a continue (dbc()) or a
 *    step (dbs()) command is then issued.  The current record's
 *    contents may be printed either with dbp(), or immediately
 *    after processing (use dbap() to toggle the BKPT_PRINT bit).
 *
 *  dbb() and dbd() add a breakpoint to a record or delete one
 *    from a record.  dbstat() prints out comprehensive breakpoint
 *    status information.
 *   
 *  Breakpoints may be set on a per lockset basis.  When a
 *    breakpoint is set in a lockset, a new task is created. A
 *    separate task gets created for _every_ lockset containing
 *    a breakpoint.  Thus multiple locksets may be debugged
 *    simultaneously.  The breakpoint handler then schedules future
 *    processing in that lockset to this task.  The separate task is
 *    used so that locksets that do not have breakpoints are isolated
 *    from locksets that do.  This allows the processing of other
 *    locksets to continue uninterupted, even if they exist on the same
 *    scan list as a lockset containing a breakpoint.
 *
 *  An entrypoint is the first record that gets processed in a lockset.
 *    This type of record is the basis for subsequent recursive executions
 *    of dbProcess().  The breakpoint handler monitors and schedules
 *    these entrypoints to the breakpoint tasks.
 *
 *  Two hooks have been inserted in dbProcess() to manage breakpoints,
 *    dbBkpt() and dbPrint().  The former does two things:
 *
 *        1.  Schedule entrypoints with the breakpoint task.
 *        2.  Suspend record processing when a breakpoint is detected.
 *
 *    1 occurs only if dbProcess() is called outside of the breakpoint
 *    task.  Number 2 only occurs when dbProcess() is called from
 *    _within_ the breakpoint task's context.  Number 1 is used for
 *    detection and scheduling, while 2 is used for suspending the task.
 *
 *  The dbPrint() hook is used to print out a record's contents immediately
 *    _after_ a record has been processed.
 *
 *  The dbBkptCont, or breakpoint task, pends on a semaphore that gets
 *    released whenever new entrypoints are scheduled for it.  When
 *    released, this task then runs down its entrypoint queue and
 *    processes each entrypoint in turn.  In this context, dbProcess
 *    will execute the dbBkpt() hook in mode 2, allowing this task to
 *    be suspended whenever a breakpoint is detected.
 *
 *  NOTE:  This is not a very "real-time" implementation (even for those
 *     locksets not containing a breakpoint).  I may fix this later.
 *
 *  Final comment:  The scary thing is, I don't think this can be done
 *     more simply...
 *
 */

/*
 *  Flag used by dbProcess() to determine if there are
 *    any breakpoints.  This is so that there is only
 *    a single comparison in the critical path during
 *    normal record execution, i.e. when there aren't
 *    any breakpoints set.
 */
long lset_stack_count = 0;

/*
 *  Stack--in which each entry represents a different
 *    lock set with either breakpoints and/or stopped
 *    execution.  (Breakpoints may be disabled even
 *    though execution is stopped).  The order of the
 *    list is maintained so that the entry on the top
 *    of stack is used as a default for dbc() and dbs().
 *    The semaphore is used to prevent conflicts while
 *    operating with this stack.
 */
static ELLLIST lset_stack = ELLLIST_INIT;
static epicsMutexId bkpt_stack_sem = 0;

/*
 *  Stores the last lockset continued or stepped from.
 *    dbs() and dbc() will print a message if the current
 *    lockset to be continued from differs from this
 *    variable.
 */
static unsigned long last_lset = 0;

/*
 *  FIND_LOCKSET() finds the stack entry
 *    whose l_num field matches precord's
 *    lset field.  The node that is found
 *    is returned in "pnode."
 */
#define FIND_LOCKSET(precord, pnode) \
  pnode = (struct LS_LIST *) ellFirst(&lset_stack); \
  while ((pnode) != NULL) { \
     if (pnode->l_num == dbLockGetLockId(precord)) break; \
     pnode = (struct LS_LIST *) ellNext((ELLNODE *)pnode); \
  } \

/*
 *  FIND_QUEUE_ENTRY() matches entries in an
 *     entry point queue.  pep_queue is the queue
 *     being searched, pqe is the pointer to the
 *     queue entry found, and precord is the record
 *     being searched for in *pep_queue.
 */
#define FIND_QUEUE_ENTRY(pep_queue, pqe, precord) \
  pqe = (struct EP_LIST *) ellFirst(pep_queue); \
  while ((pqe) != NULL) { \
     if ((pqe)->entrypoint == (precord)) break; \
     pqe = (struct EP_LIST *) ellNext((ELLNODE *)pqe); \
  } \

/*
 *  Fills out pnode and precord structures for dbc() and dbs()
 *      MUST LOCK OUT STACK BEFORE ENTRY
 */
static long FIND_CONT_NODE(
  const char *record_name,
  struct LS_LIST **ppnode,
  struct dbCommon **pprecord)
{
  struct dbAddr addr; 
  struct LS_LIST *pnode;
  struct dbCommon *precord = NULL;
  long status = 0;

  if (record_name == NULL) {
    /*
     *  Search through stack, taking the first entry that
     *    is currently stopped at a breakpoint.
     */
     pnode = (struct LS_LIST *) ellFirst(&lset_stack); 
     while (pnode != NULL) {
        if (pnode->precord != NULL) {
           precord = pnode->precord;
           break;
        }
        pnode = (struct LS_LIST *) ellNext((ELLNODE *)pnode);
     }
  
     if (pnode == NULL) {
        printf("   BKPT> No records are currently stopped\n");
        return(S_db_notStopped);
     }
  }
  else {
    /*
     *  Convert name to address
     */
     status = dbNameToAddr(record_name, &addr);
     if (status == S_db_notFound)
         printf("   BKPT> Record %s not found\n", record_name);
     if (status != 0)
         return(status);

     precord = addr.precord;

     FIND_LOCKSET(precord, pnode);

     if (pnode == NULL || pnode->precord == NULL) {
        printf("   BKPT> Currently not stopped in this lockset\n");
        return(S_db_notStopped);
     }
  }

  *pprecord = precord;
  *ppnode = pnode;
  return(0);
}

/*
 *  Initialise the breakpoint stack
 */
void dbBkptInit(void)
{
    if (! bkpt_stack_sem) {
        bkpt_stack_sem = epicsMutexMustCreate();
        lset_stack_count = 0;
    }
}

/*
 *  Add breakpoint to a lock set
 *     1. Convert name to address and check breakpoint mask.
 *     2. Lock database.
 *     3. If empty, initialize lock set stack and its semaphore.
 *     4. Take that semaphore.
 *     5. Find lockset in the list.  If it doesn't exist, create it.
 *     6. Turn on breakpoint field in record.
 *     7. Add breakpoint to list of breakpoints in structure.
 *     8. Spawn continuation task if it isn't already running.
 */
long dbb(const char *record_name)
{
  struct dbAddr addr;
  struct LS_LIST *pnode;
  struct BP_LIST *pbl;
  struct dbCommon *precord;
  long status;

 /*
  *  Convert name to address
  */
  status = dbNameToAddr(record_name, &addr);
  if (status == S_db_notFound)
      printf("   BKPT> Record %s not found\n", record_name);
  if (status != 0) return(status);

  precord = addr.precord;

  if (precord->bkpt & BKPT_ON_MASK) {
     printf("   BKPT> Breakpoint already set in this record\n"); 
     return(S_db_bkptSet);
  }

  dbScanLock(precord);

 /*
  *  Add lock set to the stack of lock sets that
  *    contain breakpoints and/or stopped records.
  */

  epicsMutexMustLock(bkpt_stack_sem);

  FIND_LOCKSET(precord, pnode);

  if (pnode == NULL) {
    /* lockset not found, create node, add to end of list */
     pnode = (struct LS_LIST *) malloc(sizeof(struct LS_LIST));
     if (pnode == NULL) {
        printf("   BKPT> Out of memory\n");
        dbScanUnlock(precord);
        epicsMutexUnlock(bkpt_stack_sem);
        return(1);
     }
     pnode->precord = NULL;

    /* initialize breakpoint list */
     ellInit(&pnode->bp_list);

    /* initialize entry point queue */
     ellInit(&pnode->ep_queue);

    /* create execution semaphore */
     pnode->ex_sem = epicsEventCreate(epicsEventEmpty);
     if (pnode->ex_sem == NULL) {
        printf("   BKPT> Out of memory\n");
        dbScanUnlock(precord);
        epicsMutexUnlock(bkpt_stack_sem);
        return(1);
     }

     pnode->taskid   = 0;
     pnode->step     = 0;
     pnode->l_num    = dbLockGetLockId(precord);

     ellAdd(&lset_stack, (ELLNODE *)pnode);
     ++lset_stack_count;
  }

 /*
  *  Add record to breakpoint list
  */
  pbl = (struct BP_LIST *) malloc(sizeof(struct BP_LIST));
  if (pbl == NULL) {
     printf("  BKPT> Out of memory\n");
     dbScanUnlock(precord);
     epicsMutexUnlock(bkpt_stack_sem);
     return(1);
  }
  pbl->precord = precord; 
  ellAdd(&pnode->bp_list, (ELLNODE *)pbl);

 /*
  *  Turn on breakpoint field in record
  */
  precord->bkpt |= BKPT_ON_MASK; 

  if (! pnode->taskid) {

#ifdef BKPT_DIAG
     printf("   BKPT> Spawning task: %s\n", precord->name);
#endif
    /*
     *  Spawn continuation task
     */
     pnode->taskid = epicsThreadCreate("bkptCont",epicsThreadPriorityScanLow-1,
         epicsThreadGetStackSize(epicsThreadStackBig),
         (EPICSTHREADFUNC)dbBkptCont,precord);
     if (pnode->taskid == 0) {
        printf("   BKPT> Cannot spawn task to process record\n");
        pnode->taskid = 0;
        dbScanUnlock(precord);
        epicsMutexUnlock(bkpt_stack_sem);
        return(1);
     }
  }

  epicsMutexUnlock(bkpt_stack_sem);
  dbScanUnlock(precord);
  return(0);
}

/*
 *  Remove breakpoint from a record
 *     1. Convert name to address and check breakpoint mask.
 *     2. Lock database and take stack semaphore.
 *     3. Find structure for record's lockset (in stack).
 *     4. Find and delete record from breakpoint list.
 *     5. Turn off break point field.
 *     6. Give up semaphore to "signal" bkptCont task to quit.
 */
long dbd(const char *record_name)
{
  struct dbAddr addr;
  struct LS_LIST *pnode;
  struct BP_LIST *pbl;
  struct dbCommon *precord;
  long status;

 /*
  *  Convert name to address
  */
  status = dbNameToAddr(record_name, &addr);
  if (status == S_db_notFound)
      printf("   BKPT> Record %s not found\n", record_name);
  if (status != 0) return(status);

  precord = addr.precord;

  if (! precord->bkpt & BKPT_ON_MASK) {
      printf("   BKPT> No breakpoint set in this record\n");
      return(S_db_bkptNotSet);
  }

  dbScanLock(precord);

  epicsMutexMustLock(bkpt_stack_sem);

  FIND_LOCKSET(precord, pnode);

  if (pnode == NULL) {
     /* not found, error ! */
     printf("   BKPT> Logic Error in dbd()\n");
     precord->bkpt &= BKPT_OFF_MASK;

     epicsMutexUnlock(bkpt_stack_sem);
     dbScanUnlock(precord);
     return(S_db_bkptLogic);
  }

 /*
  *  Remove record from breakpoint list
  */

 /* find record in list */
  pbl = (struct BP_LIST *) ellFirst(&pnode->bp_list);
  while (pbl != NULL) {
     if (pbl->precord == precord) {
         ellDelete(&pnode->bp_list, (ELLNODE *)pbl);
         free(pbl);
         break;
     }
     pbl = (struct BP_LIST *) ellNext((ELLNODE *)pbl);
  }

  if (pbl == NULL) {
     printf("   BKPT> Logic Error in dbd()\n"); 
     precord->bkpt &= BKPT_OFF_MASK;
     epicsMutexUnlock(bkpt_stack_sem);
     dbScanUnlock(precord);
     return(S_db_bkptLogic);
  }

 /*
  *  Turn off breakpoint field in record
  */
  precord->bkpt &= BKPT_OFF_MASK;

 /*
  *  If there are no more breakpoints, give up semaphore
  *    to cause the bkptCont task to quit.
  */
  if (ellCount(&pnode->bp_list) == 0)
     epicsEventSignal(pnode->ex_sem);

  epicsMutexUnlock(bkpt_stack_sem);

  dbScanUnlock(precord);
  return(0);
}

/*
 *  Continue processing in a lock set
 *     1. Find top node in the lockset stack.
 *     2. Turn off stepping mode.
 *     2. Resume dbBkptCont.
 */
long dbc(const char *record_name)
{
  struct LS_LIST *pnode;
  struct dbCommon *precord = NULL;
  long status = 0;

  epicsMutexMustLock(bkpt_stack_sem);

  status = FIND_CONT_NODE(record_name, &pnode, &precord);
  if (status) {
     epicsMutexUnlock(bkpt_stack_sem);
     return(status);
  }

  if (record_name == NULL && last_lset != pnode->l_num)
      printf("   BKPT> Continuing:  %s\n", pnode->precord->name);

  last_lset = pnode->l_num;

 /*
  *  Turn off stepping mode
  */
  pnode->step = 0;

 /*
  *  Resume dbBkptCont() until dbProcess() is executed
  *    for a record with a breakpoint.  This occurs
  *    because stepping mode has been switched off.
  */
  epicsThreadResume(pnode->taskid);
  epicsMutexUnlock(bkpt_stack_sem);
  return(0);
}

/*
 *  Step through record processing
 *     1. Find top node in lockset stack.
 *     2. Resume dbBkptCont.
 */
long dbs(const char *record_name)
{
  struct LS_LIST *pnode;
  struct dbCommon *precord = NULL;
  long status = 0;

  epicsMutexMustLock(bkpt_stack_sem);

  status = FIND_CONT_NODE(record_name, &pnode, &precord);
  if (status) {
     epicsMutexUnlock(bkpt_stack_sem);
     return(status);
  }

  if (last_lset != pnode->l_num && record_name == NULL)
      printf("   BKPT> Stepping:    %s\n", pnode->precord->name);

  last_lset = pnode->l_num;

  epicsThreadResume(pnode->taskid);
  epicsMutexUnlock(bkpt_stack_sem);
  return(0);
}

/*
 *  Task for continuing record processing
 *     1. Find lockset in stack for precord.
 *       DO 2-3 while breakpoints exist in the lockset.
 *        2. Wait on execution semaphore ...
 *        3. Run through every entrypoint in queue, processing
 *             those that are scheduled.
 *     4. Free resources for lockset, and exit task.
 */
static void dbBkptCont(dbCommon *precord)
{
  struct LS_LIST *pnode;
  struct EP_LIST *pqe = NULL;

 /*
  *  Reset breakpoint, process record, and
  *    reset bkpt field in record
  */
  epicsMutexMustLock(bkpt_stack_sem);

  FIND_LOCKSET(precord, pnode);

  if (pnode == NULL) {
    printf("   BKPT> Logic error in dbBkptCont()\n");
    return;
  }

 /*
  *  For every entrypoint scheduled, process.  Run process
  *    until there are no more breakpoints remaining in a
  *    lock set.
  */
  do {
   /* Give up semaphore before waiting to run ... */
    epicsMutexUnlock(bkpt_stack_sem);

   /* Wait to run */
    epicsEventMustWait(pnode->ex_sem);

   /* Bkpt stack must still be stable ! */
    epicsMutexMustLock(bkpt_stack_sem);

    pqe = (struct EP_LIST *) ellFirst(&pnode->ep_queue);

   /* Run through entrypoint queue */
    while (pqe != NULL) {
        /* check if entrypoint is currently scheduled */
         if (pqe->sched) {
             /* save current entrypoint */
              pnode->current_ep = pqe->entrypoint;

             /* lock the lockset, process record, unlock */
              dbScanLock(precord);
              dbProcess(pqe->entrypoint);
              dbScanUnlock(precord);

             /* reset schedule and stepping flag - Do this AFTER processing */
              pqe->sched = 0;
              pnode->step = 0;
         }
         pqe = (struct EP_LIST *) ellNext((ELLNODE *)pqe);
    }

   /* Reset precord. (Since no records are at a breakpoint) */
    pnode->precord = NULL;
  } while (ellCount(&pnode->bp_list) != 0);

 /* remove node from lockset stack */
  ellDelete(&lset_stack, (ELLNODE *)pnode);
  --lset_stack_count;

 /* free entrypoint queue */
  ellFree(&pnode->ep_queue);

 /* remove execution semaphore */
  epicsEventDestroy(pnode->ex_sem);

  printf("\n   BKPT> End debug of lockset %lu\n-> ", pnode->l_num);

 /* free list node */
  free(pnode);

  epicsMutexUnlock(bkpt_stack_sem);
}

/*
 * Process breakpoint
 *  Returns a zero if dbProcess() is to execute
 *  record support, a one if dbProcess() is to
 *  skip over record support.  See dbProcess().
 *
 *  1.  See if there is at least a breakpoint set somewhere
 *        in precord's lockset.  If not, return immediately.
 *  2.  Check the disable flag.
 *  3.  Add entry points to the queue for future stepping and
 *        schedule new entrypoints for the continuation task.
 *  4.  Check the pact flag.
 *  5.  Check to see if there is a breakpoint set in a record, and
 *        if so, turn on stepping mode.
 *  6.  If stepping mode is set, stop and report the breakpoint.
 */
int dbBkpt(dbCommon *precord)
{
  struct LS_LIST *pnode;
  struct EP_LIST *pqe;

 /*
  *  It is crucial that operations in dbBkpt() execute
  *    in the correct order or certain features in the
  *    breakpoint handler will not work as expected.
  */

 /*
  *  Take and give a semaphore to check for breakpoints
  *	every time a record is processed.  Slow.  Thank
  *	goodness breakpoint checking is turned off during
  *	normal operation.
  */
  epicsMutexMustLock(bkpt_stack_sem);
  FIND_LOCKSET(precord, pnode);
  epicsMutexUnlock(bkpt_stack_sem);

  if (pnode == NULL) {
    /* no breakpoints in precord's lockset */
     return(0);
  }

 /* Check disable flag */
  dbGetLink(&(precord->sdis),DBR_SHORT,&(precord->disa),0,0);
  if (precord->disa == precord->disv) {
     /*
      *  Do not process breakpoints if the record is disabled,
      *    but allow disable alarms.  Alarms will be raised
      *    in dbProcess() because returning 0 allows dbProcess()
      *    to continue.  However processing will be prevented
      *    because disa and disv will be examined again in
      *    dbProcess(). Note that checking for pact will occur
      *    before checking for disa and disv in dbProcess().
      */
      return(0);
  }

 /*
  *  Queue entry points for future stepping.  The taskid comparison
  *    is used to determine if the source of processing is the
  *    continuation task or an external source. If it is an external
  *    source, queue its execution, but dump out of dbProcess without
  *    calling record support. 
  */
  if (pnode->taskid && (epicsThreadGetIdSelf() != pnode->taskid)) {
    /* CONTINUE TASK CANNOT ENTER HERE */

    /*
     *  Add an entry point to queue, if it does
     *    not already exist.
     */
     FIND_QUEUE_ENTRY(&pnode->ep_queue, pqe, precord);
 
     if (pqe == NULL) {

        pqe = (struct EP_LIST *) malloc(sizeof(struct EP_LIST));
        if (pqe == NULL)
             return(1);


        pqe->entrypoint = precord;
        pqe->count = 1;
        epicsTimeGetCurrent(&pqe->time);
        pqe->sched = 0;

#ifdef BKPT_DIAG
        printf("   BKPT> Adding entrypoint %s to queue\n", precord->name);
#endif

       /*
        *  Take semaphore, wait on continuation task
        */
        epicsMutexMustLock(bkpt_stack_sem);

       /* Add entry to queue */
        ellAdd(&pnode->ep_queue, (ELLNODE *)pqe);

        epicsMutexUnlock(bkpt_stack_sem);
     }
     else {
        if (pqe->count < MAX_EP_COUNT)
           pqe->count++;
     }

    /* check pact */
     if (! precord->pact) {
       /* schedule if pact not set */
        pqe->sched = 1;

       /*
        *  Release the semaphore, letting the continuation
        *     task begin execution of the new entrypoint.
        */ 
        epicsEventSignal(pnode->ex_sem);
     }
     return(1);
  }

 /*
  *  Don't mess with breakpoints if pact set!  Skip
  *    over rest of dbProcess() since we don't want
  *    alarms going off.  The pact flag is checked
  *    AFTER entry point queuing so that the record
  *    timing feature will work properly.
  */
  if (precord->pact)
     return(1);

 /* Turn on stepping mode if a breakpoint is found */
  if (precord->bkpt & BKPT_ON_MASK) {
     pnode->step = 1;

#ifdef BKPT_DIAG
     printf("   BKPT> Bkpt detected: %s\n", precord->name);
#endif
  }

 /*
  *  If we are currently stepping through the lockset,
  *    suspend task.
  */
  if (pnode->step) {
      printf("\n   BKPT> Stopped at:  %s  within Entrypoint:  %s\n-> ",
                 precord->name, pnode->current_ep->name);

      pnode->precord = precord;

     /* Move current lockset to top of stack */
      ellDelete(&lset_stack, (ELLNODE *)pnode);
      ellInsert(&lset_stack, NULL, (ELLNODE *)pnode);
     /*
      *  Unlock database while the task suspends itself.  This
      *   is done so that dbb() dbd() dbc() dbs() may be used
      *   when the task is suspended.  Scan tasks that also
      *   use the scan lock feature will not be hung during
      *   a breakpoint, so that records in other locksets will
      *   continue to be processed.  Cross your fingers, this
      *   might actually work !
      */
      epicsMutexUnlock(bkpt_stack_sem);
      dbScanUnlock(precord);
      epicsThreadSuspendSelf();
      dbScanLock(precord);
      epicsMutexMustLock(bkpt_stack_sem);
   }
   return(0);
}

/* print record after processing */
void dbPrint(dbCommon *precord )
{
  struct LS_LIST *pnode;

  if (! (precord->bkpt & BKPT_PRINT_MASK))
     return;

  FIND_LOCKSET(precord, pnode);

 /* do not print if lockset does not currently contain breakpoints */
  if (pnode == NULL)
     return;

  printf("\n");
  dbpr(precord->name, 2);
  printf("-> ");
}

/* print stopped record */
long dbp(const char *record_name, int interest_level)
{
  struct LS_LIST *pnode;
  struct dbCommon *precord = NULL;
  int status;

  epicsMutexMustLock(bkpt_stack_sem);

 /* find pnode and precord pointers */
  status = FIND_CONT_NODE(record_name, &pnode, &precord);
  if (status) {
     epicsMutexUnlock(bkpt_stack_sem);
     return(status);
  }

 /* print out record's fields */
  dbpr(precord->name, (interest_level == 0) ? 2 : interest_level);

  epicsMutexUnlock(bkpt_stack_sem);
  return(0);
}

/* toggle printing after processing a certain record */
long dbap(const char *record_name)
{
  struct dbAddr addr;
  struct dbCommon *precord;
  long status;

 /*
  *  Convert name to address
  */
  status = dbNameToAddr(record_name, &addr);
  if (status == S_db_notFound)
      printf("   BKPT> Record %s not found\n", record_name);
  if (status != 0) return(status);

  precord = addr.precord;

 /*
  *  Toggle print after process field in record
  */
  if (precord->bkpt & BKPT_PRINT_MASK) {
     printf("   BKPT> Auto print off for record %s\n", precord->name);
     precord->bkpt &= BKPT_PRINT_OFF_MASK;
  }
  else {
     printf("   BKPT> Auto print on for record %s\n", precord->name);
     precord->bkpt |= BKPT_PRINT_MASK;
  }

  return(0);
}

/* print list of stopped records, and breakpoints set in locksets */
long dbstat(void)
{
  struct LS_LIST *pnode;
  struct BP_LIST *pbl;
  struct EP_LIST *pqe;
  epicsTimeStamp time;

  epicsMutexMustLock(bkpt_stack_sem);

  epicsTimeGetCurrent(&time);

 /*
  *  Traverse list, reporting stopped records
  */
  pnode = (struct LS_LIST *) ellFirst(&lset_stack);
  while (pnode != NULL) {
    if (pnode->precord != NULL) {

       printf("LSet: %lu  Stopped at: %-28.28s  #B: %5.5d  T: %p\n",
             pnode->l_num, pnode->precord->name, ellCount(&pnode->bp_list), pnode->taskid);

      /* for each entrypoint detected, print out entrypoint statistics */
       pqe = (struct EP_LIST *) ellFirst(&pnode->ep_queue); 
       while (pqe != NULL) {
          double diff = epicsTimeDiffInSeconds(&time,&pqe->time);
          if (diff) {
             printf("             Entrypoint: %-28.28s  #C: %5.5lu  C/S: %7.1f\n",
                 pqe->entrypoint->name, pqe->count,diff);
          }
          pqe = (struct EP_LIST *) ellNext((ELLNODE *)pqe);
       }
    }
    else {
       printf("LSet: %lu                                            #B: %5.5d  T: %p\n",
         pnode->l_num, ellCount(&pnode->bp_list), pnode->taskid);
    }

   /*
    *  Print out breakpoints set in the lock set
    */
    pbl = (struct BP_LIST *) ellFirst(&pnode->bp_list);
    while (pbl != NULL) {
        printf("             Breakpoint: %-28.28s", pbl->precord->name);

       /* display auto print flag */
        if (pbl->precord->bkpt & BKPT_PRINT_MASK)
           printf(" (ap)\n");
        else
           printf("\n");

        pbl = (struct BP_LIST *) ellNext((ELLNODE *)pbl);
    }

    pnode = (struct LS_LIST *) ellNext((ELLNODE *)pnode);
  }

  epicsMutexUnlock(bkpt_stack_sem);
  return(0);
}

/*
 *  Process a record without printing it.
 */
long dbprc(char *record_name)
{
  struct dbAddr addr;
  struct dbCommon *precord;
  long status;

 /*
  *  Convert name to address
  */
  status = dbNameToAddr(record_name, &addr);
  if (status == S_db_notFound)
      printf("   BKPT> Record %s not found\n", record_name);
  if (status != 0) return(status);

  precord = addr.precord;

 /* lock lockset, process record, unlock lockset */
  dbScanLock(precord);
  status = dbProcess(precord);
  dbScanUnlock(precord);

  return(status);
}

#ifdef BKPT_DIAG

/* Reset breakpoints */
int dbreset()
{
  epicsMutexUnlock(bkpt_stack_sem);

  return(0);
}

#endif

