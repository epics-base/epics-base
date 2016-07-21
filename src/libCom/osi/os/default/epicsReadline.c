/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
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
#include "epicsExit.h"
#include "epicsReadline.h"

#define EPICS_COMMANDLINE_LIBRARY_EPICS     0
#define EPICS_COMMANDLINE_LIBRARY_LIBTECLA  1
#define EPICS_COMMANDLINE_LIBRARY_READLINE  2
#define EPICS_COMMANDLINE_LIBRARY_READLINE_CURSES  2
#define EPICS_COMMANDLINE_LIBRARY_READLINE_NCURSES 2

#ifndef EPICS_COMMANDLINE_LIBRARY
#define EPICS_COMMANDLINE_LIBRARY EPICS_COMMANDLINE_LIBRARY_EPICS
#endif



#if EPICS_COMMANDLINE_LIBRARY == EPICS_COMMANDLINE_LIBRARY_LIBTECLA
#include <libtecla.h>
#include <string.h>

/*
 * Create a command-line context
 */
void * epicsShareAPI
epicsReadlineBegin (FILE *in)
{
    GetLine *gl;
    long i = 50;

    envGetLongConfigParam(&IOCSH_HISTSIZE, &i);
    if (i < 0) i = 0;
    gl = new_GetLine(200, i * 40);
    if ((gl != NULL) && (in != NULL))
        gl_change_terminal(gl, in, stdout, NULL);
    return gl;
}

/*
 * Read a line of input
 */
char * epicsShareAPI
epicsReadline (const char *prompt, void *context)
{
    char *line;
    char *nl;

    line = gl_get_line(context, prompt ? prompt : "", NULL, -1);
    if ((line != NULL) && ((nl = strchr(line, '\n')) != NULL))
        *nl = '\0';
    return line;
}

/*
 * Destroy a command-line context
 */
void epicsShareAPI
epicsReadlineEnd(void *context)
{
    del_GetLine(context);
}


#elif EPICS_COMMANDLINE_LIBRARY == EPICS_COMMANDLINE_LIBRARY_READLINE

#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>

struct readlineContext {
    FILE    *in;
    char    *line;
};

static enum {rlNone, rlIdle, rlBusy} rlState = rlNone;

static void rlExit(void *dummy) {
    if (rlState == rlBusy)
        rl_cleanup_after_signal();
}


/*
 * Create a command-line context
 */
void * epicsShareAPI
epicsReadlineBegin(FILE *in)
{
    struct readlineContext *readlineContext;

    if (rlState == rlNone) {
        epicsAtExit(rlExit, NULL);
        rlState = rlIdle;
    }

    readlineContext = malloc(sizeof *readlineContext);
    if (readlineContext != NULL) {
        readlineContext->in = in;
        readlineContext->line = NULL;
        if (in == NULL) {
            long i = 50;

            envGetLongConfigParam(&IOCSH_HISTSIZE, &i);
            if (i < 0) i = 0;
            stifle_history (i);
            rl_bind_key ('\t', rl_insert);
        }
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

    int c;      /* char is unsigned on some archs, EOF is -ve */
    char *line = NULL;
    int linelen = 0;
    int linesize = 50;

    free (readlineContext->line);
    readlineContext->line = NULL;
    if (readlineContext->in == NULL) {
        if (!isatty(fileno(stdout))) {
            /* The libedit readline emulator on Darwin doesn't
             * print the prompt when the terminal isn't a tty.
             */
            fputs (prompt, stdout);
            fflush (stdout);
            rl_already_prompted = 1;
        }
        rlState = rlBusy;
        line = readline (prompt);
        rlState = rlIdle;
    }
    else {
        line = (char *)malloc (linesize * sizeof *line);
        if (line == NULL) {
            printf ("Out of memory!\n");
            return NULL;
        }
        if (prompt) {
            fputs (prompt, stdout);
            fflush (stdout);
        }
        while ((c = getc (readlineContext->in)) !=  '\n') {
            if (c == EOF) {
                free (line);
                line = NULL;
                break;
            }
            if ((linelen + 1) >= linesize) {
                char *cp;
    
                linesize += 50;
                cp = (char *)realloc (line, linesize * sizeof *line);
                if (cp == NULL) {
                    printf ("Out of memory!\n");
                    free (line);
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
    readlineContext->line = line;
    if (line && line[0] != '\0')
        add_history (line);
    return line;
}

/*
 * Destroy a command-line context
 */
void epicsShareAPI
epicsReadlineEnd (void *context)
{
    struct readlineContext *readlineContext = context;
    
    if (readlineContext) {
        free(readlineContext->line);
        free(readlineContext);
    }
}


#elif EPICS_COMMANDLINE_LIBRARY == EPICS_COMMANDLINE_LIBRARY_EPICS

#if defined(vxWorks)

#include <string.h>
#include <ledLib.h>
#define LEDLIB_LINESIZE 1000

#ifndef _WRS_VXWORKS_MAJOR
typedef int LED_ID;
#endif

struct readlineContext {
    LED_ID  ledId;
    char    line[LEDLIB_LINESIZE];
    FILE    *in;
};

/*
 * Create a command-line context
 */
void * epicsShareAPI
epicsReadlineBegin(FILE *in)
{
    struct readlineContext *readlineContext;

    readlineContext = malloc(sizeof *readlineContext);
    if (readlineContext != NULL) {
        readlineContext->ledId = (LED_ID) ERROR;
        readlineContext->in = in;
        if (in == NULL) {
            long i = 50;

            envGetLongConfigParam(&IOCSH_HISTSIZE, &i);
            if (i < 1) i = 1;
            readlineContext->ledId = ledOpen(fileno(stdin), fileno(stdout), i);
            if (readlineContext->ledId == (LED_ID) ERROR) {
                readlineContext->in = stdin;
                printf("Warning -- Unabled to allocate space for command-line history.\n");
                printf("Warning -- Command-line editting disabled.\n");
            }
        }
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
    int i;

    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }
    if (readlineContext->ledId != (LED_ID) ERROR) {
        i = ledRead(readlineContext->ledId, readlineContext->line, LEDLIB_LINESIZE-1); 
        if (i < 0)
            return NULL;
    }
    else {
        if (fgets(readlineContext->line, LEDLIB_LINESIZE, readlineContext->in) == NULL)
            return NULL;
        i = strlen(readlineContext->line);
    }
    if ((i >= 1) && (readlineContext->line[i-1] == '\n'))
        readlineContext->line[i-1] = '\0';
    else
        readlineContext->line[i] = '\0';
    return readlineContext->line;
}

/*
 * Destroy a command-line context
 */
void epicsShareAPI
epicsReadlineEnd (void *context)
{
    struct readlineContext *readlineContext = context;
    
    if (readlineContext) {
        if (readlineContext->ledId != (LED_ID) ERROR)
            ledClose(readlineContext->ledId);
        free(readlineContext);
    }
}

#else /* !vxWorks */

struct readlineContext {
    FILE    *in;
    char    *line;
};

/*
 * Create a command-line context
 */
void * epicsShareAPI
epicsReadlineBegin(FILE *in)
{
    struct readlineContext *readlineContext;
    
    readlineContext = malloc(sizeof *readlineContext);
    if (readlineContext != NULL) {
        readlineContext->in = in;
        readlineContext->line = NULL;
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

    int c;      /* char is unsigned on some archs, EOF is -ve */
    char *line = NULL;
    int linelen = 0;
    int linesize = 50;
    FILE *in;

    free (readlineContext->line);
    readlineContext->line = NULL;
    if ((in = readlineContext->in) == NULL) {
        in = stdin;
        if (prompt != NULL) {
            fputs (prompt, stdout);
            fflush (stdout);
        }
    }
    line = (char *)malloc (linesize * sizeof *line);
    if (line == NULL) {
        printf ("Out of memory!\n");
        return NULL;
    }
    while ((c = getc (in)) !=  '\n') {
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
            cp = (char *)realloc (line, linesize * sizeof *line);
            if (cp == NULL) {
                printf ("Out of memory!\n");
                free (line);
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
    struct readlineContext *readlineContext = context;
    
    if (readlineContext) {
        free(readlineContext->line);
        free(readlineContext);
    }
}

#endif /* !vxWorks */

#else

# error "Unsupported EPICS_COMMANDLINE_LIBRARY"

#endif
