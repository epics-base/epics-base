/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocsh.cpp */
/* Author:  Marty Kraimer Date: 27APR2000 */
/* Heavily modified by Eric Norum   Date: 03MAY2000 */
/* Adapted to C++ by Eric Norum   Date: 18DEC2000 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "errlog.h"
#include "dbAccess.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "registry.h"
#define epicsExportSharedSymbols
#include "iocsh.h"
#include "epicsReadline.h"

/*
 * File-local information
 */
struct iocshCommand {
    iocshFuncDef const     *pFuncDef;
    iocshCallFunc          func;
    struct iocshCommand    *next;
};
static struct iocshCommand *iocshCommandHead;
static char iocshID[] = "iocsh";
static epicsMutexId commandTableMutex;
static epicsThreadOnceId commandTableOnceId = EPICS_THREAD_ONCE_INIT;

/*
 * Set up command table mutex
 */
extern "C" {
static void commandTableOnce (void *)
{
    commandTableMutex = epicsMutexMustCreate ();
}
}

/*
 * Lock command table mutex
 */
static void
commandTableLock (void)
{
    epicsThreadOnce (&commandTableOnceId, commandTableOnce, NULL);
    epicsMutexMustLock (commandTableMutex);
}

/*
 * Unlock the command table mutex
 */
static void
commandTableUnlock (void)
{
    epicsThreadOnce (&commandTableOnceId, commandTableOnce, NULL);
    epicsMutexUnlock (commandTableMutex);
}

/*
 * Register a command
 */
void epicsShareAPI iocshRegister (const iocshFuncDef *piocshFuncDef, iocshCallFunc func)
{
    struct iocshCommand *l, *p, *n;
    int i;

    commandTableLock ();
    for (l = NULL, p = iocshCommandHead ; p != NULL ; l = p, p = p->next) {
        i = strcmp (piocshFuncDef->name, p->pFuncDef->name);
        if (i == 0) {
            p->pFuncDef = piocshFuncDef;
            p->func = func;
            commandTableUnlock ();
            return;
        }
        if (i < 0)
            break;
    }
    n = (struct iocshCommand *)callocMustSucceed (1, sizeof *n, "iocshRegister");
    if (!registryAdd(iocshID, piocshFuncDef->name, (void *)n)) {
        free (n);
        commandTableUnlock ();
        errlogPrintf ("iocshRegister failed to add %s\n", piocshFuncDef->name);
        return;
    }
    if (l == NULL) {
        n->next = iocshCommandHead;
        iocshCommandHead = n;
    }
    else {
        n->next = l->next;
        l->next = n;
    }
    n->pFuncDef = piocshFuncDef;
    n->func = func;
    commandTableUnlock ();
}

/*
 * Free storage created by iocshRegister
 */
void epicsShareAPI iocshFree(void) 
{
    struct iocshCommand *p, *n;

    commandTableLock ();
    for (p = iocshCommandHead ; p != NULL ; ) {
        n = p->next;
        free (p);
        p = n;
    }
    commandTableUnlock ();
}

/*
 * Report an error
 */
static void
showError (const char *filename, int lineno, const char *msg, ...)
{
    va_list ap;

    va_start (ap, msg);
    if (filename)
        fprintf (stderr, "%s -- Line %d -- ", filename, lineno);
    vfprintf (stderr, msg, ap);
    fputc ('\n', stderr);
    va_end (ap);
}

static int
cvtArg (const char *filename, int lineno, char *arg, iocshArgBuf *argBuf, const iocshArg *piocshArg)
{
    char *endp;

    switch (piocshArg->type) {
    case iocshArgInt:
        if (arg && *arg) {
            argBuf->ival = strtol (arg, &endp, 0);
            if (*endp) {
                showError (filename, lineno, "Illegal integer `%s'", arg);
                return 0;
            }
        }
        else {
            argBuf->ival = 0;
        }
        break;

    case iocshArgDouble:
        if (arg && *arg) {
            argBuf->dval = strtod (arg, &endp);
            if (*endp) {
                showError (filename, lineno, "Illegal double `%s'", arg);
                return 0;
            }
        }
        else {
            argBuf->dval = 0.0;
        }
        break;

    case iocshArgString:
        argBuf->sval = arg;
        break;

    case iocshArgPdbbase:
        /* Argument must be missing or 0 or pdbbase */
        if(!arg || !*arg || (*arg == '0') || (strcmp(arg, "pdbbase") == 0)) {
            argBuf->vval = pdbbase;
            break;
        }
        showError (filename, lineno, "Expecting `pdbbase' got `%s'", arg);
        return 0;

    default:
        showError (filename, lineno, "Illegal argument type %d", piocshArg->type);
        return 0;
    }
    return 1;
}

/*
 * The body of the command interpreter
 */
int epicsShareAPI
iocsh (const char *pathname)
{
    FILE *fp = NULL;
    const char *filename = NULL;
    int icin, icout;
    char c;
    int quote, inword, backslash;
    char *line = NULL;
    int lineno = 0;
    int argc;
    char **argv = NULL;
    int argvCapacity = 0;
    int sep;
    const char *prompt;
    const char *ifs = " \t(),";
    iocshArgBuf *argBuf = NULL;
    int argBufCapacity = 0;
    struct iocshCommand *found;
    struct iocshFuncDef const *piocshFuncDef;
    
    /*
     * See if command interpreter is interactive
     */
    if ((pathname == NULL) || (strcmp (pathname, "<telnet>") == 0)) {
        const char *historySize;
        if ((prompt = getenv ("IOCSH_PS1")) == NULL)
            prompt = "epics> ";
        if (((historySize = getenv ("IOCSH_HISTSIZE")) == NULL)
         && ((historySize = getenv ("HISTSIZE")) == NULL))
            historySize = "20";
        if (pathname == NULL) {
            epicsStifleHistory (atoi (historySize));
            /*
             * FIXME: Could enable tab-completion of commands here
             */
            epicsBindKeys();
        }
        else {
            fp = stdin;
        }
    }
    else {
        fp = fopen (pathname, "r");
        if (fp == NULL) {
            fprintf (stderr, "Can't open %s: %s\n", pathname, strerror (errno));
            return -1;
        }
        if ((filename = strrchr (pathname, '/')) == NULL)
            filename = pathname;
        else
            filename++;
        prompt = NULL;
    }

    /*
     * Read commands till EOF or exit
     */
    for (;;) {
        /*
         * Get a line
         */
        lineno++;
        free (line);
        line = epicsReadline (fp, prompt);
        if (line == NULL)
            break;

        /*
         * If using readline, add non-blank lines to history
         */
        if ((fp == NULL) && *line)
            epicsAddHistory (line);

        /*
         * Ignore comment lines
         */
        if (*line == '#')
            continue;

	if ((prompt == NULL) && *line)
	    puts(line);

        /*
         * Break line into words
         */
        icout = icin = 0;
        inword = 0;
        quote = EOF;
        backslash = 0;
        argc = 0;
        for (;;) {
            if (argc >= argvCapacity) {
                char **av;
                argvCapacity += 50;
                av = (char **)realloc (argv, argvCapacity * sizeof *argv);
                if (av == NULL) {
                    printf ("Out of memory!\n");
                    argc = -1;
                    break;
                }
                argv = av;
            }
            c = line[icin++];
            if (c == '\0')
                break;
            if ((quote == EOF) && !backslash && (strchr (ifs, c)))
                sep = 1;
            else
                sep = 0;
            if ((quote == EOF) && (c == '\\') && !backslash) {
                backslash = 1;
                continue;
            }
            if (inword) {
                if (c == quote) {
                    quote = EOF;
                }
                else {
                    if ((quote == EOF) && !backslash) {
                        if (sep) {
                            inword = 0;
                            line[icout++] = '\0';
                        }
                        else if ((c == '"') || (c == '\'')) {
                            quote = c;
                        }
                        else {
                            line[icout++] = c;
                        }
                    }
                    else {
                        line[icout++] = c;
                    }
                }
            }
            else {
                if (!sep) {
                    if (((c == '"') || (c == '\'')) && !backslash)
                        quote = c;
                    argv[argc++] = line + icout;
                    if (quote == EOF)
                        line[icout++] = c;
                    inword = 1;
                }
            }
            backslash = 0;
        }
        if (argc < 0)
            break;
        if (quote != EOF) {
            showError (filename, lineno, "Unbalanced quote.");
            continue;
        }
        if (backslash) {
            showError (filename, lineno, "Trailing backslash.");
            continue;
        }
        if (inword)
            line[icout++] = '\0';
        argv[argc] = NULL;

        /*
         * Look up command
         */
        if (argc) {
            /*
             * Special command?
             */
            if (strncmp (argv[0], "exit", 4) == 0)
                break;
            if ((strcmp (argv[0], "?") == 0) 
             || (strncmp (argv[0], "help", 4) == 0)) {
                if (argc == 1) {
                    int l, col = 0;

                    printf ("Type `help command_name' to get more information about a particular command.\n");
                    commandTableLock ();
                    for (found = iocshCommandHead ; found != NULL ; found = found->next) {
                        piocshFuncDef = found->pFuncDef;
                        l = strlen (piocshFuncDef->name);
                        if ((l + col) >= 79) {
                            putchar ('\n');
                            col = 0;
                        }
                        fputs (piocshFuncDef->name, stdout);
                        col += l;
                        if (col >= 64) {
                            putchar ('\n');
                            col = 0;
                        }
                        else {
                            do {
                                putchar (' ');
                                col++;
                            } while ((col % 16) != 0);
                        }
                    }
                    if (col)
                        putchar ('\n');
                    commandTableUnlock ();
                }
                else {
                    for (int i = 1 ; i < argc ; i++) {
                        found = (iocshCommand *)registryFind (iocshID, argv[i]);
                        if (found == NULL) {
                            printf ("%s -- no such command.\n", argv[i]);
                        }
                        else {
                            piocshFuncDef = found->pFuncDef;
                            fputs (piocshFuncDef->name, stdout);
                            for (int a = 0 ; a < piocshFuncDef->nargs ; a++) {
                                const char *cp = piocshFuncDef->arg[a]->name;
                                if ((piocshFuncDef->arg[a]->type == iocshArgArgv)
                                 || (strchr (cp, ' ') == NULL)) {
                                    printf (" %s", cp);
                                }
                                else {
                                    printf (" '%s'", cp);
                                }
                            }
                            putchar ('\n');
                        }
                    }
                }
                continue;
            }

            /*
             * Look up command
             */
            found = (iocshCommand *)registryFind (iocshID, argv[0]);
            if (!found) {
                showError (filename, lineno, "Command %s not found.", argv[0]);
                continue;
            }
            piocshFuncDef = found->pFuncDef;

            /*
             * Process arguments and call function
             */
            for (int arg = 0 ; ; arg++) {
                if (arg == piocshFuncDef->nargs) {
                    (*found->func)(argBuf);
                    break;
                }
                if (arg >= argBufCapacity) {
                    void *np;

                    argBufCapacity += 20;
                    np = realloc (argBuf, argBufCapacity * sizeof *argBuf);
                    if (np == NULL) {
                        fprintf (stderr, "Out of memory!\n");
                        argBufCapacity -= 20;
                        break;
                    }
                    argBuf = (iocshArgBuf *)np;
                }
                if (piocshFuncDef->arg[arg]->type == iocshArgArgv) {
                    argBuf[arg].aval.ac = argc-arg;
                    argBuf[arg].aval.av = argv+arg;
                    (*found->func)(argBuf);
                    break;
                }
                if (!cvtArg (filename, lineno,
                                        ((arg < argc) ? argv[arg+1] : NULL),
                                        &argBuf[arg], piocshFuncDef->arg[arg]))
                    break;
            }
        }
    }
    if (fp && (fp != stdin))
        fclose (fp);
    free (line);
    free (argv);
    free (argBuf);
    errlogFlush();
    return 0;
}

/*
 * Dummy internal commands -- register and install in command table
 * so they show up in the help display
 */

extern "C" {
/* help */
static const iocshArg helpArg0 = { "command",iocshArgInt};
static const iocshArg *helpArgs[1] = {&helpArg0};
static const iocshFuncDef helpFuncDef =
    {"help",1,helpArgs};
static void helpCallFunc(const iocshArgBuf *)
{
}

/* comment */
static const iocshArg commentArg0 = { "newline-terminated comment",iocshArgArgv};
static const iocshArg *commentArgs[1] = {&commentArg0};
static const iocshFuncDef commentFuncDef = {"#",1,commentArgs};
static void commentCallFunc(const iocshArgBuf *)
{
}

/* exit */
static const iocshFuncDef exitFuncDef =
    {"exit",0,0};
static void exitCallFunc(const iocshArgBuf *)
{
}

static void localRegister (void)
{
    iocshRegister(&helpFuncDef,helpCallFunc);
    iocshRegister(&commentFuncDef,helpCallFunc);
    iocshRegister(&exitFuncDef,exitCallFunc);
}

} /* extern "C" */

/*
 * Register commands on application startup
 */
#include "iocshRegisterCommon.h"
class IocshRegister {
  public:
    IocshRegister() { localRegister(); iocshRegisterCommon(); }
};
static IocshRegister iocshRegisterObj;
