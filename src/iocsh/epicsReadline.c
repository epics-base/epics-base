/* epicsReadline.c */
/* Author:  Eric Norum Date: 18DEC2000 */

/*
 * Wrappers around readline calls to work around the fact that
 * on many machines readline.h declares functions with old-style
 * (i.e. non-prototype) syntax.  The old syntax is incompatible
 * with C++.
 */

#include <stdio.h>
#include "epicsReadline.h"

#ifdef IOCSH_USE_READLINE

#include <readline/readline.h>
#include <readline/history.h>

#else

/*
 * Fake versions of some readline/history routines
 */
#define readline(p) do { } while(0)
#define stifle_history(n) do { } while(0)
#define add_history(l) do { } while(0)
#define rl_bind_key(c,f) do { } while(0)

#endif

char *epics_readline (const char *prompt)
{
    return readline (prompt);
}

void epics_stifle_history (int n)
{
    stifle_history (n);
}

void epics_add_history (const char *line)
{
    add_history (line);
}

void epics_bind_keys (void)
{
    rl_bind_key ('\t', rl_insert);
}
