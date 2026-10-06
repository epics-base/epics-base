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
#include <string.h>

#include "envDefs.h"
#include "epicsAssert.h"
#include "epicsReadlinePvt.h"

#ifndef SA_RESTART
// request BSD compatible EINTR handling from Linux
#  define SA_RESTART (0)
#endif

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

#define CHECK(EXPECT, EXPR) do { \
    int actual = EXPR; \
    if(actual!=(EXPECT)) { \
        int err = errno; \
        fprintf(stderr, "%s:%d %s %d==%d (errno %d)\n", __FILE__, __LINE__, #EXPR, EXPECT, actual, err); \
        assert(0); \
    } \
} while(0)

#ifdef SA_RESETHAND
static
struct readlineContext *activeContext;

static
void close_stdin(int signo)
{
    // SA_RESETHAND ensures this handler will only be called once.
    (void)signo;
    // replace stdin with /dev/null
    // replacement process will interrupt or restart concurrent syscall on stdin
    // attenpt to retry read of /dev/null will yield EoF
    CHECK(STDIN_FILENO, dup2(activeContext->devnull, STDIN_FILENO));
}

static
void erlInstallHandler(struct readlineContext *rc)
{
    assert(activeContext==NULL);
    activeContext = rc;

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_flags = SA_RESETHAND | SA_RESTART;
    act.sa_handler = &close_stdin;
    CHECK(0, sigaction(SIGINT, &act, &rc->sigint));

    assert(rc->sigint.sa_handler != &close_stdin); // refuse to recurse after restore
}

static
void erlRestoreHandler(struct readlineContext *rc)
{
    assert(activeContext==rc);
    CHECK(0, sigaction(SIGINT, &rc->sigint, NULL));
    activeContext=NULL;

    // restore original stdin (even if no substitution was made
    CHECK(0, dup2(rc->clone_stdin, STDIN_FILENO));
}

#else
#  define erlInstallHandler(RC) do {(void)(RC);}while(0)
#  define erlRestoreHandler(RC) do {(void)(RC);}while(0)

#endif /* SA_RESETHAND */

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

#ifdef SA_RESETHAND
        rc->devnull = open("/dev/null", O_RDONLY);
        rc->clone_stdin = dup(STDIN_FILENO);
        assert(rc->devnull>=0 && rc->clone_stdin>=0);
#endif
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
    char *line = NULL;
    int c;      /* char is unsigned on some archs, EOF is -ve */
    int linelen = 0;
    int linesize = 50;
    int backslash_seen = 0;

    erlInstallHandler(rc);

    if (rc->osd) {
        line = osdReadline(prompt, rc);
        goto done;
    }

    free(rc->line);
    rc->line = NULL;
    if ((in = rc->in) == NULL) {
        in = stdin;
        if (prompt) {
            fputs(prompt, stdout);
            fflush(stdout);
        }
    }
    rc->line = line = (char *)malloc(linesize);
    if (line == NULL) {
        goto done;
    }
    do {
        c = getc(in);
        if (c == EOF) {
            if (ferror(in)) {
                if ((errno == EINTR) || (errno == EPIPE)) {
                    clearerr(in);
                    continue;
                }
            }
            goto error;
        }
        if ((linelen + 1) >= linesize) {
            char *cp;

            linesize += 50;
            cp = (char *)realloc(line, linesize);
            if (cp == NULL) {
                printf("Out of memory!\n");
                goto error;
            }
            rc->line = line = cp;
        }
        if (backslash_seen) {
            /* try to handle multi-line string */
            backslash_seen = 0;
            if (c == '\n') {
                linelen--;      /* overwrite the '\' */
                c = getc(in);   /* skip current '\n' and get the next char */
                if (c == EOF) {
                    goto error; // actually normal EOF
                }
            }
        }
        if (c == '\\') {
            backslash_seen = 1;
        }
        if (c != '\n') {
            line[linelen++] = c;
        }
    } while (c != '\n');
    line[linelen] = '\0';
done:
    erlRestoreHandler(rc);
    return line;
error:
    free(line);
    line = NULL;
    rc->line = line = NULL;
    goto done;
}

/*
 * Destroy a command-line context
 */
void epicsStdCall
epicsReadlineEnd (void *context)
{
    if (context) {
        struct readlineContext *rc = context;

#ifdef SA_RESETHAND
        CHECK(0, close(rc->devnull));
        CHECK(0, close(rc->clone_stdin));
        rc->clone_stdin = rc->devnull = -1;
#endif

        if (rc->osd)
            osdReadlineEnd(rc);
        else
            free(rc->line);
        free(rc);
    }
}

