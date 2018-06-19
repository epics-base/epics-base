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
#include <libtecla.h>

struct osdContext {};

/*
 * Create a command-line context
 */
static void
osdReadlineBegin (struct readlineContext *context)
{
    GetLine *gl;
    long i = 50;

    envGetLongConfigParam(&IOCSH_HISTSIZE, &i);
    if (i < 0)
        i = 0;

    gl = new_GetLine(200, i * 40);
    if (gl) {
        context->osd = (struct osdContext *) gl;
        if (context->in)
            gl_change_terminal(gl, context->in, stdout, NULL);
    }
}

/*
 * Read a line of input
 */
static char *
osdReadline (const char *prompt, struct readlineContext *context)
{
    GetLine *gl = (GetLine *) context->osd;
    char *line;

    line = gl_get_line(gl, prompt ? prompt : "", NULL, -1);
    if (line) {
        char *nl = strchr(line, '\n');

        if (nl)
            *nl = '\0';
    }
    return line;
}

/*
 * Destroy a command-line context
 */
static void
osdReadlineEnd(struct readlineContext *context)
{
    GetLine *gl = (GetLine *) context->osd;

    del_GetLine(gl);
}

