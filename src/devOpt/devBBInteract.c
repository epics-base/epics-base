/* devBBInteract.c */
/* share/src/devOpt $Id$
 *
 *      Author: Ned D. Arnold
 *      Date:   06/19/91
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1988, 1989, the Regents of the University of California,
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
 * All rights reserved. No part of this publication may be reproduced,
 * stored in a retrieval system, transmitted, in any form or by any
 * means,  electronic, mechanical, photocopying, recording, or otherwise
 * without prior written permission of Los Alamos National Laboratory
 * and Argonne National Laboratory.
 *
 * Modification Log:
 * -----------------
 * .01  06-19-91        nda     initial coding & debugging
 * .02  09-23-91        jrw     added proper multi-link support
 * .03	10-04-91	jrw	rewrote as BI.c for use with BitBus
 *
 */

#include        <vxWorks.h>
#include        <sysLib.h>
#include        <iosLib.h>
#include        <types.h>
#include        <stdioLib.h>
#include        <strLib.h>
#include        <lstLib.h>
#include        <rngLib.h>
#include        <taskLib.h>

#include        <alarm.h>
#include        <cvtTable.h>
#include        <dbDefs.h>
#include        <dbAccess.h>
#include        <devSup.h>
#include        <drvSup.h>
#include        <link.h>
#include        <module_types.h>
#include        <dbCommon.h>
#include	<fast_lock.h>

#include	<drvBitBusInterface.h>

extern struct {
  long number;
  DRVSUPFUN	report;
  DRVSUPFUN	init;
  DRVSUPFUN	qReq;
} drvBitBus;

struct	cmdPvt {
  int		busy;
  long int	count;
};

#define LIST_SIZE	10

static struct dpvtBitBusHead	adpvt[LIST_SIZE];

/* declare other required variables used by more than one routine */

extern int bbDebug;
static int	replyIsBack;

int	sendMsg();
int	bbWork();
int	configMsg();
int	getInt();
int	getChar();
int	getString();
int	showBbMsg();
int	timingStudy();

static int firstTime = 1;
static FAST_LOCK msgReply;

int
BI()
{
  unsigned char	ans;
  int	cnt;

  if(firstTime)
  {
    firstTime = 0;
    FASTLOCKINIT(&msgReply);
    FASTUNLOCK(&msgReply);
    FASTLOCK(&msgReply);	/* Make sure is locked at the begining */
    
    for (cnt = 0; cnt < LIST_SIZE; cnt++)
    {
      adpvt[cnt].finishProc = bbWork;
      adpvt[cnt].priority = 0;

      adpvt[cnt].next = NULL;
      adpvt[cnt].prev = NULL;
      adpvt[cnt].txMsg.data = (unsigned char *)malloc(BB_MAX_DAT_LEN);
      adpvt[cnt].txMsg.length = 7;
      adpvt[cnt].txMsg.route = 0x40;
      adpvt[cnt].rxMsg.data = (unsigned char *)malloc(BB_MAX_DAT_LEN);
      adpvt[cnt].rxMsg.length = 7;
      adpvt[cnt].syncLock = NULL;
      adpvt[cnt].rxMaxLen = BB_MAX_MSG_LENGTH;
    }
  }

  ans = 0;                /* set loop not to exit */
  printf("\n\n");

  while ((ans != 'q') && (ans != 'Q'))
  {
    printf("\n\nInteractive BB Program\n");
    printf("Select a function:\n");     /* main menu */
    printf("  'C' to configure send message content\n");
    printf("  'D' to display transmit & receive messages\n");
    printf("  'S' to send & receive a message one time\n");
    printf("  'T' to do timing on messages\n");
    printf("  'R' to turn on the BB debugging flag\n");
    printf("  'r' to turn off the BB debugging flag\n");
    printf("  'Q' to quit program\n");
    printf(">");
    ans = 0;    /* clear previous selection */
    getChar(&ans);

    switch (ans) {
    case 'c':       /* configure message contents */
    case 'C':
      configMsg();
      break;

    case 's':
    case 'S':       /* one shot: send one message */
      sendMsg();
      break;

    case 't':
    case 'T':       /* Timing analysis */
      /*timingStudy();*/
      break;

    case 'd':
    case 'D':       /* Display message contents */
      for (cnt = 1; cnt < LIST_SIZE; cnt++) /* for each message */
      {
	printf("message %d Transmit buffer:\n", cnt);
        showBbMsg(&(adpvt[cnt].txMsg));
	printf("message %d Receive buffer:\n", cnt);
	showBbMsg(&(adpvt[cnt].rxMsg));
      }
      break;

      case 'q':
      case 'Q':       /* quit */
        break;

      case 'r':		/* turn off bbDebug */
        bbDebug = 0;
        break;
      case 'R':		/* turn on bbDebug */
        bbDebug = 1;
        break;
    }               /* end case */
  }                   /* end of while ans== q or Q  */
  return(0);
}                       /* end of main */

#ifdef DONT_DO_THIS
static int
timingStudy()
{
  struct bbIntCmd *pCmd[LIST_SIZE];
  int	inInt;          /* input integer from operator */
  int	reps;		/* number of times to send a message */
  ULONG	startTime;
  ULONG	endTime;
  ULONG	delta;
  int	Reps;
  int	i;
  int	j;

  for (i=0; i<LIST_SIZE; i++)
  {
    printf("\nEnter Message# to Send (1 thru 5, <cr> to terminate list) > ");
    if (getInt(&inInt))
    {
      if (inInt>0 && inInt<LIST_SIZE)
      {
        pCmd[i] = &bbIntCmds[inInt];    /* assign pointer to desired entry */
        pCmd[i]->busy = 0;		/* mark message 'not in queue' */
	pCmd[i]->count = 0;
      }
      else
      {
        pCmd[i] = NULL;			/* terminate the list */
	i = LIST_SIZE;
      }
    }
    else
    {
      pCmd[i] = NULL;                 /* terminate the list */
      i = LIST_SIZE;
    }
  }

  printf("Enter the number of messages to send > ");
  if (!getInt(&inInt))
    return;             /* if no entry, return to main menu */
  if(inInt < 0)
    return;
  reps = inInt;
  Reps = reps;

  startTime = tickGet();	/* time the looping started */

  while(reps)
  {
    for(i=0; i<LIST_SIZE && reps; i++)
    {
      if (pCmd[i] != NULL)
      {
        if (!pCmd[i]->busy)
        {
	  pCmd[i]->count++;
	  pCmd[i]->busy = 1;	/* mark the xact as busy */
          (*(drvBitBus.qReq))(pCmd[i]->link, &(pCmd[i]), BB_Q_HIGH);
	  reps--;
	  if (reps%10000 == 0)
	  {
  printf("Timing study in progress with %d messages left, current status:\n", reps);
  for (j=0; j<LIST_SIZE; j++)
  {
    if(pCmd[j] != NULL)
    {
      printf("Message %d transmissions: %ld\n", j, pCmd[j]->count);
    }
    else
      j = LIST_SIZE;
  }

	  }
        }
      }
      else
	i = LIST_SIZE;		/* force an exit from the loop */
    }
    FASTLOCK(&msgReply);
  }

  endTime = tickGet();
  delta = (endTime - startTime);  /* measured time in ticks  */
  if (delta == 0)
    delta = 1;
  printf("Total transmit and receiving time = %ld/60 Sec\n", delta);
  printf("Messages/second = %.2f\n", (float) (60*Reps)/(float)delta);

  for (i=0; i<LIST_SIZE; i++)
  {
    if(pCmd[i] != NULL)
    {
      printf("Message %d transmissions: %ld\n", i, pCmd[i]->count);
    }
    else
      i = LIST_SIZE;
  }
  return(OK);
}
#endif

/* sendMsg() ***************************************************
 */

static int
sendMsg()
{
  int             msgNum;         /* index to array of messages */


  printf("\nEnter Message # to Send (1 thru 5) > ");
  if (!getInt(&msgNum))
    return;             /* if no entry, return to main menu */
  if((msgNum >= LIST_SIZE) || (msgNum < 0))
    return;

  adpvt[msgNum].ageLimit = 10;	/* need to reset each time */
  (*(drvBitBus.qReq))(&(adpvt[msgNum]), BB_Q_HIGH); /* queue the msg */

  FASTLOCK(&msgReply);	/* wait for response to return */

  printf("response message:\n");
  showBbMsg(&(adpvt[msgNum].rxMsg));
}

static int
bbWork(pdpvt)
struct dpvtBitBusHead *pdpvt;
{
  if (bbDebug)
    printf("BI's bbWork():entered\n");

  replyIsBack = TRUE;
  FASTUNLOCK(&msgReply);
  return(0);
}

/*--------------------------------------------------------------------
 * configMsg()
 *
 * - Purpose
 *      Prompts the operator for the contents of a BB message
 *      Displays current value of the field as the default entry
 *
 */

static int
configMsg()
{
  int	msgNum;         /* index to array of messages */
  int	inInt;
  int	cnt;	
  char	str[100];

  printf("\nEnter Message # to Configure (1 thru 5) > ");
  if (!getInt(&inInt))
    return;             /* if no entry, return to main menu */
  if((inInt >= LIST_SIZE) || (inInt < 0))
    return;

  msgNum = inInt;

  printf("\n\n Configuring Send Message # %d at 0x%08.8X \n", msgNum, &(adpvt[msgNum].txMsg));

/* Prompt the Operator with the current value of each parameter If
 * only a <CR> is typed, keep current value, else replace value with
 * entered value
 */

  adpvt[msgNum].txMsg.link = 0;

  printf("Enter BB Link (hex) [%02.2X]: ", (int) adpvt[msgNum].link);
  gets(str);
  if (sscanf(str, "%x", &inInt) == 1)
    adpvt[msgNum].link = inInt;

  printf("Enter route   (hex) [%02.2X]: ", adpvt[msgNum].txMsg.route);
  gets(str);
  if (sscanf(str, "%x", &inInt) == 1)
    adpvt[msgNum].txMsg.route = inInt;

  printf("Enter Node    (hex) [%02.2X]: ", adpvt[msgNum].txMsg.node);
  gets(str);
  if (sscanf(str, "%x", &inInt) == 1)
    adpvt[msgNum].txMsg.node = inInt;

  printf("Enter tasks   (hex) [%02.2X]: ", adpvt[msgNum].txMsg.tasks);
  gets(str);
  if (sscanf(str, "%x", &inInt) == 1)
    adpvt[msgNum].txMsg.tasks = inInt;

  printf("Enter command (hex) [%02.2X]: ", adpvt[msgNum].txMsg.cmd);
  gets(str);
  if (sscanf(str, "%x", &inInt) == 1)
    adpvt[msgNum].txMsg.cmd = inInt;

  adpvt[msgNum].txMsg.length = 7;
  printf("Enter data 1 byte per line. Enter a dot to terminate list (hex)\n");
  for (cnt=0; cnt<BB_MAX_DAT_LEN; cnt++)
  {
    printf("[%02.2X]: ", adpvt[msgNum].txMsg.data[cnt]);
    gets(str);
    if (str[0] == '.')
      break;
    if (sscanf(str, "%x", &inInt) == 1)
      adpvt[msgNum].txMsg.data[cnt] = inInt;
    adpvt[msgNum].txMsg.length++;
  }
  printf("Transmit message #%d for BitBus link %d:\n", msgNum, adpvt[msgNum].link);
  showBbMsg(&(adpvt[msgNum].txMsg));
}

/*
 * getInt(pInt) ****************************************************
 *
 * gets a line of input from STDIN (10 characters MAX !!!!) scans the line
 * for an integer.
 *
 * Parm1 : pointer to an integer
 *
 * Returns (0) if a <CR> was hit, *pInt left unchanged. Returns (1) if a new
 * value was written to *pvalue. Returns (2) if a entry was not a valid
 * integer entry
 *
 */

static int
getInt(pInt)
int  *pInt;
{
  char    input[10];
  int     inval;

  gets(input);

  if (input[0] == 0)      /* if only a <CR> */
    return (0);
  else
  {
    if (sscanf(input, "%d", &inval) == 1)
    {               /* if a valid entry ... */
      *pInt = inval;
      return (1);
    }
    else                /* if not a valid entry, return 2 */
      return (2);
  }
}

/*
 * int getChar(pChar) **************************************************
 *
 * gets a line of input from STDIN (10 characters MAX !!!!) scans the line
 * for a character entry
 *
 * Parm1 : pointer to an unsigned character variable
 *
 * Returns(0) if a <CR> was hit, *pChar left unchanged. Returns(1) if a new
 * value was written to *pChar
 *
 */

static int
getChar(pChar)
unsigned char *pChar;
{
  char    input[10];
  unsigned char   inval;

  gets(input);

  if (input[0] == 0)
    return (0);
  else
  {
    sscanf(input, "%c", &inval);
    *pChar = inval;
    return (1);
  }
}

/*
 * int getString(pString) ****************************************************
 *
 * gets a line of input from STDIN (100 characters MAX !!!!) .
 * If length is greater than 0, copies string into pString
 *
 * Parm1 : pointer to an character string
 *
 * Returns : 0 if a <CR> was hit, *pvalue left unchanged Returns : 1 if a
 * new value was written to *pvalue
 *
 */

static int
getString(pString)
char    *pString;
{
  char    input[100];

  gets(input);

  if (input[0] == 0)
    return (0);
  else
  {
    strcpy(pString, input);
    return (1);
  }
}

/*
 * Print the bb message contents onto the screen.
 * msgNum selects the message to be displayed
 */

static int
showBbMsg(msg)
struct bitBusMsg *msg;
{
  int	i;

  printf("    link=%04.4X length=%02.2X route=%02.2X node=%02.2X tasks=%02.2X cmd=%02.2X\n",
        msg->link, msg->length, msg->route, msg->node, msg->tasks, msg->cmd);
  printf("    data :");
  for (i = 0; i < msg->length - 7; i++)
  {
    printf(" %02.2X", msg->data[i]);
  }
  putchar('\n');
}
