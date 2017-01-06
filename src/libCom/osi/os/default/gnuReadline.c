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

#include <readline/readline.h>
#include <readline/history.h>

#include "epicsExit.h"

static struct osdContext {
    char dummy; /* Required for older compilers */
} present;

static enum {rlNone, rlIdle, rlBusy} rlState = rlNone;

static void rlExit(void *dummy) {
    if (rlState == rlBusy)
        rl_cleanup_after_signal();
}


/*
 * Create a command-line context
 */
static void
osdReadlineBegin(struct readlineContext *context)
{
    if (rlState == rlNone) {
        epicsAtExit(rlExit, NULL);
        rlState = rlIdle;
    }

    context->osd = &present;
    if (context->in == NULL) {
        long i = 50;

        envGetLongConfigParam(&IOCSH_HISTSIZE, &i);
        if (i < 0)
            i = 0;
        stifle_history(i);
        rl_bind_key('\t', rl_insert);
    }
}

/*
 * Read a line of input
 */
static char *
osdReadline (const char *prompt, struct readlineContext *context)
{
    char *line;

    free(context->line);
    context->line = NULL;
    if (context->in == NULL) {
        rlState = rlBusy;
        line = readline (prompt);
        rlState = rlIdle;
    }
    else {
        int c;      /* char is unsigned on some archs; EOF is -ve */
        int linelen = 0;
        int linesize = 50;

        line = malloc(linesize);
        if (line == NULL) {
            printf("Out of memory!\n");
            return NULL;
        }
        if (prompt) {
            fputs(prompt, stdout);
            fflush(stdout);
        }
        while ((c = getc(context->in)) !=  '\n') {
            if (c == EOF) {
                free(line);
                line = NULL;
                break;
            }
            if ((linelen + 1) >= linesize) {
                char *cp;
    
                linesize += 50;
                cp = (char *)realloc(line, linesize);
                if (cp == NULL) {
                    printf ("Out of memory!\n");
                    free(line);
                    line = NULL;
                    break;
                }
                line = cp;
            }
            line[linelen++] = c;
        }
        if (line)
            line[linelen] = '\0';
    }
    context->line = line;
    if (line && *line)
        add_history(line);
    return line;
}

/*
 * Destroy a command-line context
 */
static void
osdReadlineEnd (struct readlineContext *context)
{
    if (context->osd) {
        free(context->line);
    }
}

