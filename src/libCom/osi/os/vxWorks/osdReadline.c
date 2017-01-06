/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* Author:  Eric Norum Date: 12DEC2001 */

/*
 * This file is included by epicsReadline.c which has already included the
 * headers stdio.h, stdlib.h, errno.h, envDefs.h and epicsReadline.h
 */

#include <string.h>
#include <ledLib.h>

/* FIXME: Remove line-lenth limitation */
#define LEDLIB_LINESIZE 1000

#ifndef _WRS_VXWORKS_MAJOR
typedef int LED_ID;
#endif

struct osdContext {
    LED_ID  ledId;
    char    line[LEDLIB_LINESIZE];
};

/*
 * Create a command-line context
 */
static void
osdReadlineBegin(struct readlineContext *context)
{
    struct osdContext osd = malloc(sizeof *osd);

    if (osd != NULL) {
        osd->ledId = (LED_ID) ERROR;
        if (context->in == NULL) {
            long i = 50;

            envGetLongConfigParam(&IOCSH_HISTSIZE, &i);
            if (i < 1)
                i = 1;

            osd->ledId = ledOpen(fileno(stdin), fileno(stdout), i);
            if (osd->ledId == (LED_ID) ERROR) {
                context->in = stdin;
                printf("Warning -- Unabled to allocate space for command-line history.\n");
                printf("Warning -- Command-line editting disabled.\n");
            }
        }
        context->osd = osd;
    }
}

/*
 * Read a line of input
 */
static char *
osdReadline (const char *prompt, struct readlineContext *context)
{
    struct osdContext *osd = context->osd;
    int i;

    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }
    if (osd->ledId != (LED_ID) ERROR) {
        i = ledRead(osd->ledId, osd->line, LEDLIB_LINESIZE-1); 
        if (i < 0)
            return NULL;
    }
    else {
        if (fgets(osd->line, LEDLIB_LINESIZE, context->in) == NULL)
            return NULL;
        i = strlen(osd->line);
    }
    if ((i >= 1) && (osd->line[i-1] == '\n'))
        osd->line[i-1] = '\0';
    else
        osd->line[i] = '\0';
    return osd->line;
}

/*
 * Destroy a command-line context
 */
static void
osdReadlineEnd (struct readlineContext *context)
{
    LED_ID ledId = context->osd->ledId;

    if (ledId != (LED_ID) ERROR)
        ledClose(ledId);
    free(context->osd);
}

