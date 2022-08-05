/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* Copyright (c) 2015 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* Author: Eric Norum */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "envDefs.h"
#include "epicsReadlinePvt.h"

static void osdReadlineBegin(struct readlineContext *);
static char * osdReadline(const char *prompt, struct readlineContext *);
static void osdReadlineEnd(struct readlineContext *);

#if EPICS_COMMANDLINE_LIBRARY == EPICS_COMMANDLINE_LIBRARY_EPICS

static void osdReadlineBegin(struct readlineContext *rc) {}
static char * osdReadline(const char *prompt, struct readlineContext *rc)
{
    return NULL;
}
static void osdReadlineEnd(struct readlineContext *rc) {}

#elif EPICS_COMMANDLINE_LIBRARY == EPICS_COMMANDLINE_LIBRARY_READLINE
#  include "gnuReadline.c"
#else
#  include "osdReadline.c"
#endif

/*
 * Create a command-line context
 */
void * epicsStdCall
epicsReadlineBegin(FILE *in)
{
    struct readlineContext *rc = calloc(1, sizeof(*rc));

    if (rc) {
        rc->in = in;
        rc->line = NULL;
        if (!envGetConfigParamPtr(&IOCSH_HISTEDIT_DISABLE))
            osdReadlineBegin(rc);
    }
    return rc;
}

/*
 * Read a line of input
 */
char * epicsStdCall
epicsReadline (const char *prompt, void *context)
{
    struct readlineContext *rc = context;
    FILE *in;
    char *line;
    int c;      /* char is unsigned on some archs, EOF is -ve */
    int linelen = 0;
    int linesize = 50;

    if (rc->osd)
        return osdReadline(prompt, rc);

    free(rc->line);
    rc->line = NULL;
    if ((in = rc->in) == NULL) {
        in = stdin;
        if (prompt) {
            fputs(prompt, stdout);
            fflush(stdout);
        }
    }
    line = (char *)malloc(linesize);
    if (line == NULL) {
        printf("Out of memory!\n");
        return NULL;
    }
    while ((c = getc(in)) !=  '\n') {
        if (c == EOF) {
            if (ferror(in)) {
                if ((errno == EINTR) || (errno == EPIPE)) {
                    clearerr(in);
                    continue;
                }
            }
            free (line);
            return NULL;
        }
        if ((linelen + 1) >= linesize) {
            char *cp;

            linesize += 50;
            cp = (char *)realloc(line, linesize);
            if (cp == NULL) {
                printf("Out of memory!\n");
                free(line);
                return NULL;
            }
            line = cp;
        }
        line[linelen++] = c;
    }
    line[linelen] = '\0';
    rc->line = line;
    return line;
}

/*
 * Destroy a command-line context
 */
void epicsStdCall
epicsReadlineEnd (void *context)
{
    if (context) {
        struct readlineContext *rc = context;

        if (rc->osd)
            osdReadlineEnd(rc);
        else
            free(rc->line);
        free(rc);
    }
}

