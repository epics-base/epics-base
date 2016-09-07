/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* Author:  Eric Norum Date: 12DEC2001 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define epicsExportSharedSymbols
#include "envDefs.h"
#include "epicsReadline.h"

#define EPICS_COMMANDLINE_LIBRARY_EPICS     0
#define EPICS_COMMANDLINE_LIBRARY_LIBTECLA  1
#define EPICS_COMMANDLINE_LIBRARY_READLINE  2
#define EPICS_COMMANDLINE_LIBRARY_READLINE_CURSES  2
#define EPICS_COMMANDLINE_LIBRARY_READLINE_NCURSES 2

#ifndef EPICS_COMMANDLINE_LIBRARY
#  define EPICS_COMMANDLINE_LIBRARY EPICS_COMMANDLINE_LIBRARY_EPICS
#endif

struct osdContext;
struct readlineContext {
    FILE    *in;
    char    *line;
    struct osdContext *osd;
};

static void osdReadlineBegin(struct readlineContext *);
static char * osdReadline(const char *prompt, struct readlineContext *);
static void osdReadlineEnd(struct readlineContext *);

#if EPICS_COMMANDLINE_LIBRARY == EPICS_COMMANDLINE_LIBRARY_EPICS

static void osdReadlineBegin(struct readlineContext * c) {}
static char * osdReadline(const char *prompt, struct readlineContext * c) { return NULL; }
static void osdReadlineEnd(struct readlineContext * c) {}

#else

#  if EPICS_COMMANDLINE_LIBRARY == EPICS_COMMANDLINE_LIBRARY_READLINE
#    include "gnuReadline.c"
#  else
#    include "osdReadline.c"
#  endif

#endif

/*
 * Create a command-line context
 */
void * epicsShareAPI
epicsReadlineBegin(FILE *in)
{
    struct readlineContext *readlineContext = calloc(1, sizeof(*readlineContext));

    if (readlineContext) {
        readlineContext->in = in;
        readlineContext->line = NULL;
        if (!envGetConfigParamPtr(&IOCSH_HISTEDIT_DISABLE))
            osdReadlineBegin(readlineContext);
    }
    return readlineContext;
}

/*
 * Read a line of input
 */
char * epicsShareAPI
epicsReadline (const char *prompt, void *context)
{
    struct readlineContext *readlineContext = context;
    FILE *in;
    char *line;
    int c;      /* char is unsigned on some archs, EOF is -ve */
    int linelen = 0;
    int linesize = 50;

    if (readlineContext->osd)
        return osdReadline(prompt, readlineContext);

    free(readlineContext->line);
    readlineContext->line = NULL;
    if ((in = readlineContext->in) == NULL) {
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
    readlineContext->line = line;
    return line;
}

/*
 * Destroy a command-line context
 */
void epicsShareAPI
epicsReadlineEnd (void *context)
{
    if (context) {
        struct readlineContext *readlineContext = context;

	if (readlineContext->osd)
	    osdReadlineEnd(readlineContext);
	else
            free(readlineContext->line);
        free(readlineContext);
    }
}

