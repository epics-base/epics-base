/* gpibInteract.c */
/* share/src/devOpt  $Id$ */
/*
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
 *
 */

/* gpibInteract.c - allows interactive GPIB message transmissions  */

/*
 * gpibInteract allows the user to interactively set up gpib messages,
 * submit them to the gpibOwn Queue, and display the result. Five
 * gpib messages can be defined at a time.
 */

#include        <vxWorks.h>
#include        <sysLib.h>
#include        <iosLib.h>
#include        <types.h>
#include        <stdioLib.h>
#include        <strLib.h>
#include        <semLib.h>
#include        <lstLib.h>
#include        <rngLib.h>
#include        <taskLib.h>

#include        <alarm.h>
#include        <cvtTable.h>
#include        <dbDefs.h>
#include        <dbAccess.h>
#include        <devSup.h>
#include	<drvSup.h>
#include        <link.h>
#include        <module_types.h>
#include        <dbCommon.h>

#include	<drvGpibInterface.h>

#define GPIBREAD        1
#define GPIBWRITE       2
#define GPIBCMD         3

#define         MAX_MSG_LENGTH  80

extern struct drvGpibSet drvGpib;       /* entry points to driver functions */
int	gpibWork();

struct gpibIntCmd {
  struct dpvtGpibHead head;

  char	type;           	/* R, W, C for read, write, command */
  char	cmd[MAX_MSG_LENGTH];    /* string to send to instrument */
  char	resp[MAX_MSG_LENGTH+32];/* place for response if a GPIB read */
  long	count;          	/* used for counting */
  char	busy;			/* used by timing routine */
};

static struct gpibIntCmd gpibIntCmds[] =
{
  {NULL, NULL, gpibWork, 0, 0, NULL, 'R', "", "", 0, 0},
  {NULL, NULL, gpibWork, 0, 0, NULL, 'R', "", "", 0, 0},
  {NULL, NULL, gpibWork, 0, 0, NULL, 'R', "", "", 0, 0},
  {NULL, NULL, gpibWork, 0, 0, NULL, 'R', "", "", 0, 0},
  {NULL, NULL, gpibWork, 0, 0, NULL, 'R', "", "", 0, 0},
  {NULL, NULL, gpibWork, 0, 0, NULL, 'R', "", "", 0, 0}
};
#define         LIST_SIZE	sizeof(gpibIntCmds)/sizeof(struct gpibIntCmd)

/* declare other required variables used by more than one routine */

BOOL            replyIsBack;    /* reset by sender, set by replyReceived() */

extern	int	ibDebug;
int		GIDebug = 0;

 int	sendMsg();
 int	gpibWork();
 int	configMsg();
 int	getInt();
 int	getChar();
 int	getString();
 int	showGpibMsg();
 int	timingStudy();

static int firstTime = 1;
static SEM_ID	msgReply;

int
GI()
{
  unsigned char	ans;
  int	cnt;

  if(firstTime)
  {
    firstTime = 0;
    msgReply = semCreate();

  }

  ans = 0;                /* set loop not to exit */
  printf("\n\n");

  while ((ans != 'q') && (ans != 'Q'))
  {
    printf("\n\nInteractive GPIB Program\n");
    printf("Select a function:\n");     /* main menu */
    printf("  'C' to configure send message content\n");
    printf("  'D' to display transmit & receive messages\n");
    printf("  'S' to send & receive a message one time\n");
    printf("  'T' to do timing on messages\n");
    printf("  'R' to turn on the GPIB debugging flag\n");
    printf("  'r' to turn off the GPIB debugging flag\n");
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
      timingStudy();
      break;

    case 'd':
    case 'D':       /* Display message contents */
      for (cnt = 1; cnt < LIST_SIZE; cnt++) /* for each message */
        showGpibMsg(cnt);
      printf("\nHit CR to return to Main Menu ...");
      getChar(&ans);      /* dummy input */
        break;

      case 'q':
      case 'Q':       /* quit */
        break;

      case 'r':		/* turn off ibDebug */
        ibDebug = 0;
        break;
      case 'R':		/* turn on ibDebug */
        ibDebug = 1;
        break;
    }               /* end case */
  }                   /* end of while ans== q or Q  */
  return(0);
}                       /* end of main */

static int
timingStudy()
{
  struct gpibIntCmd *pCmd[LIST_SIZE];
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
        pCmd[i] = &gpibIntCmds[inInt];    /* assign pointer to desired entry */
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
        if (!(pCmd[i]->busy))
        {
	  pCmd[i]->count++;
	  pCmd[i]->busy = 1;	/* mark the xact as busy */
          (*(drvGpib.qGpibReq))(GPIB_IO, pCmd[i]->head.link, pCmd[i], 2);
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
    semTake(msgReply);
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

/* sendMsg() ***************************************************
 */

static int
sendMsg()
{
  struct gpibIntCmd *pCmd;
  int             inInt;          /* input integer from operator */
  int             msgNum;         /* index to array of messages */
  int     ticks;  /* # of ticks since message was sent */
  int     maxTicks = 480; /* # of ticks to wait for reply (8 seconds) */

  printf("\nEnter Message # to Send (1 thru 5) > ");
  if (!getInt(&inInt))
    return;             /* if no entry, return to main menu */
  if((inInt >= LIST_SIZE) || (inInt < 0))
    return;

  msgNum = inInt;
  pCmd = &gpibIntCmds[msgNum];    /* assign pointer to desired entry */

  replyIsBack = FALSE;
  ticks = 0;

  (*(drvGpib.qGpibReq))(GPIB_IO, pCmd->head.link, pCmd, 2); /* queue the msg */

  while (!replyIsBack && (ticks < maxTicks))      /* wait for reply msg */
  {
    taskDelay(1);
    ticks++;
  }

  if (replyIsBack)
  {
    showGpibMsg(msgNum);
  }
  else
    printf("No Reply Received ...\n");

}

static int
gpibWork(pCmd)
struct gpibIntCmd *pCmd;
{
char    msgBuf[MAX_MSG_LENGTH + 1];
int     status;

  if (GIDebug || ibDebug)
    logMsg("GI's gpibWork() was called for command >%s<\n", pCmd->cmd);

  switch (pCmd->type) {
    case 'w':
    case 'W':         /* write the message to the GPIB listen adrs */
      status =(*(drvGpib.writeIb))(GPIB_IO, pCmd->head.link, 0, pCmd->head.device, pCmd->cmd, strlen(pCmd->cmd));
      if (status == ERROR)
	strcpy(pCmd->resp, "GPIB TIMEOUT (while talking)");
      else
        pCmd->resp[0] = '\0';
      break;
    case 'r':
    case 'R':               /* write the command string */
      status = (*(drvGpib.writeIb))(GPIB_IO, pCmd->head.link, 0, pCmd->head.device, pCmd->cmd, strlen(pCmd->cmd));
      if (status == ERROR)
      {
        strcpy(pCmd->resp, "GPIB TIMEOUT (while talking)");
        break;
      }
      /* read the instrument  */
      pCmd->resp[0] = 0;          /* clear response string */
      status = (*(drvGpib.readIb))(GPIB_IO, pCmd->head.link, 0, pCmd->head.device, pCmd->resp, MAX_MSG_LENGTH);

      if (status == ERROR)
      {
        strcat(pCmd->resp, "GPIB ERROR (while listening)");
        break;
      }
      else if (status > (MAX_MSG_LENGTH - 1)) /* check length of resp */
      {
        printf("GPIB Response length equaled allocated space !!!\n");
        pCmd->resp[(MAX_MSG_LENGTH)] = '\0';    /* place \0 at end */
      }
      else
      {
        pCmd->resp[status] = '\0'; /* terminate response with \0 */
      }
      break;
  }
  pCmd->busy = 0;
  replyIsBack = TRUE;
  semGive(msgReply);
  return(IDLE);
}

/*--------------------------------------------------------------------
 * configMsg()
 *
 * - Purpose
 *      Prompts the operator for the contents of a GPIB message
 *      Displays current value of the field as the default entry
 *
 */

static int
configMsg()
{
  struct gpibIntCmd *pCmd;
  int             msgNum;         /* index to array of messages */
  int             inInt;          /* input integer from operator */
  unsigned char   inChar;         /* input char from operator  */
  char            inString[MAX_MSG_LENGTH]; /* input string from operator */

  printf("\nEnter Message # to Configure (1 thru 5) > ");
  if (!getInt(&inInt))
    return;             /* if no entry, return to main menu */
  if((inInt >= LIST_SIZE) || (inInt < 0))
    return;

  msgNum = inInt;
  pCmd = &gpibIntCmds[msgNum];    /* assign pointer to desired entry */

  printf("\n\n Configuring Send Message # %1.1d  .... \n", msgNum);

/* Prompt the Operator with the current value of each parameter If
 * only a <CR> is typed, keep current value, else replace value with
 * entered value
 */

  printf("\nenter Enter GPIB Link # [%2.2d] > ", (int) pCmd->head.link);
  if (getInt(&inInt) == 1)
    pCmd->head.link = inInt;

  printf("\nenter GPIB Node # [%2.2d] > ", pCmd->head.device);
  if (getInt(&inInt) == 1)
  {
    pCmd->head.device = inInt;
  }

  printf("\nenter command type R, W, C [%c] > ", pCmd->type);
  if (getChar(&inChar) == 1)
    pCmd->type = inChar;

  printf("\nenter string to send (no quotes) [%.80s] > ", pCmd->cmd);
  if (getString(inString) == 1)
    strcpy(pCmd->cmd, inString);

  pCmd->resp[0]= 0;       /* clear response string */
  
  showGpibMsg(msgNum);
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
 * showGpibMsg(msgNum) ***********************************************
 *
 *
 * Print the gpib message contents onto the screen.
 * msgNum selects the message to be displayed
 *
 */

static int
showGpibMsg(msgNum)
int msgNum;

{
  struct gpibIntCmd *pCmd = &gpibIntCmds[msgNum];

  printf("\nMessage #%1.1d : ", msgNum);
  printf("Link=%2.2d Adrs=%2.2d Type=%c\n",
        (int)pCmd->head.link, pCmd->head.device, pCmd->type);
  printf("             Command String  : %.40s\n", pCmd->cmd);
  printf("             Response String : %.40s\n", pCmd->resp);
}

