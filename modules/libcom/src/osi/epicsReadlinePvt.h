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
