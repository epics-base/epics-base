/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocsh.cpp */
/* Author:  Eric Norum Date: 12DEC2001 */

#include <stdio.h>

#define epicsExportSharedSymbols

#include <epicsReadline.h>
#include <osdReadline.h>

#if (defined (IOCSH_REAL_READLINE) && defined (IOCSH_FAKE_READLINE))
# undef IOCSH_FAKE_READLINE
#endif

#if (defined (IOCSH_REAL_READLINE) || defined (IOCSH_FAKE_READLINE))

#include <stdio.h>
#include <stdlib.h>

#if (defined (IOCSH_REAL_READLINE))
# include <readline/readline.h>
# include <readline/history.h>
#endif

/*
 * Read a line of input
 */
char * epicsShareAPI
epicsReadline (FILE *fp, const char *prompt)
{
    int c;      /* char is unsigned on some archs, EOF is -ve */
    char *line = NULL;
    int linelen = 0;
    int linesize = 50;

    if (fp == NULL)
#ifdef IOCSH_REAL_READLINE
        return readline (prompt);
#else
        fp = stdin;
#endif
    line = (char *)malloc (linesize * sizeof *line);
    if (line == NULL) {
        printf ("Out of memory!\n");
        return NULL;
    }
    if (prompt) {
        fputs (prompt, stdout);
        fflush (stdout);
    }
    while ((c = getc (fp)) !=  '\n') {
        if (c == EOF) {
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
    return line;
}

void epicsShareAPI epicsStifleHistory (int n)
{
#if (defined (IOCSH_REAL_READLINE))
    stifle_history (n);
#endif
}

void epicsShareAPI epicsAddHistory (const char *line)
{
#if (defined (IOCSH_REAL_READLINE))
    add_history (line);
#endif
}

void epicsShareAPI epicsBindKeys (void)
{
#if (defined (IOCSH_REAL_READLINE))
    rl_bind_key ('\t', rl_insert);
#endif
}

#endif /* defined (IOCSH_REAL_READLINE) || defined (IOCSH_FAKE_READLINE) */
