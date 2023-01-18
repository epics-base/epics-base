/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* Copyright (c) 2015 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef EPICSREADLINEPVT_H
#define EPICSREADLINEPVT_H

#include "epicsReadline.h"

/* Basic command-line input, no editing or history: */
#define EPICS_COMMANDLINE_LIBRARY_EPICS     0

/*  OS-specific command-line editing and/or history: */
#define EPICS_COMMANDLINE_LIBRARY_LIBTECLA  1
#define EPICS_COMMANDLINE_LIBRARY_LEDLIB    1
#define EPICS_COMMANDLINE_LIBRARY_OTHER     1

/* GNU readline, or Apple's libedit wrapper: */
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

#endif // EPICSREADLINEPVT_H
