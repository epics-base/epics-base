/* ioccrf.c */
/* Author:  Marty Kraimer Date: 27APR2000 */
/* Heavily modified by Eric Norum   Date: 03MAY2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "cantProceed.h"
#include "registry.h"
#include "errlog.h"
#include "dbAccess.h"
#define epicsExportSharedSymbols
#include "ioccrf.h"

typedef struct argvalue {
    struct argvalue *next;
    union type {
        int ival;
        double dval;
    } type;
} argvalue;

typedef struct ioccrfFunc {
    ioccrfFuncDef *pioccrfFuncDef;
    ioccrfCallFunc func;
    struct ioccrfFunc *pprev;
} ioccrfFunc;

static ioccrfFunc *pioccrfFuncHead = 0;
    
static char ioccrfID[] = "ioccrf";

#ifdef IOCSH_USE_READLINE

#include <readline/readline.h>
#include <readline/history.h>

#else

/*
 * Fake versions of some readline/history routines
 */
#define stifle_history(n) do { } while(0)
#define add_history(l) do { } while(0)
#define rl_bind_key(c,f) do { } while(0)

#endif

/*
 * Register a command
 */
void epicsShareAPI ioccrfRegister (ioccrfFuncDef *pioccrfFuncDef, ioccrfCallFunc func)
{
    ioccrfFunc *pioccrfFunc;

    pioccrfFunc = callocMustSucceed (1, sizeof(ioccrfFunc), "ioccrfRegister");
    pioccrfFunc->pprev = pioccrfFuncHead;
    /* keep list of allocated ioccrfFunc so they can be freed*/
    pioccrfFuncHead = pioccrfFunc;
    pioccrfFunc->pioccrfFuncDef = pioccrfFuncDef;
    pioccrfFunc->func = func;
    if (!registryAdd(ioccrfID, pioccrfFuncDef->name, (void *)pioccrfFunc))
        errlogPrintf ("ioccrfRegister failed to add %s\n", pioccrfFuncDef->name);
}

/* free storage created by ioccrfRegister */
void epicsShareAPI ioccrfFree(void) 
{
    ioccrfFunc *pioccrfFunc = pioccrfFuncHead;
    pioccrfFuncHead = 0;
    while(pioccrfFunc) {
        ioccrfFunc *pprev = pioccrfFunc->pprev;
        free(pioccrfFunc);
        pioccrfFunc = pprev;
    }
}


/*
 * Read a line of input
 */
static char *
my_readline (FILE *fp, const char *prompt)
{
    char c;
    char *line = NULL;
    int linelen = 0;
    int linesize = 50;

#ifdef IOCSH_USE_READLINE
    if (fp == stdin)
        return readline (prompt);
#endif
    line = malloc (linesize * sizeof *line);
    if (line == NULL) {
        printf ("Out of memory!\n");
        return NULL;
    }
    if (prompt)
        printf ("%s", prompt);
    while ((c = getc (fp)) !=  '\n') {
        if (c == EOF) {
            free (line);
            return NULL;
        }
        if ((linelen + 1) >= linesize) {
            char *cp;

            linesize += 50;
            cp = realloc (line, linesize * sizeof *line);
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
    fprintf (stderr, "\n");
    va_end (ap);
}

static int
cvtArg (const char *filename, int lineno, char *arg, argvalue *pargvalue, ioccrfArg *pioccrfArg)
{
    char *endp;

    switch (pioccrfArg->type) {
    case ioccrfArgInt:
        if (arg && *arg) {
            pargvalue->type.ival = strtol (arg, &endp, 0);
            if (*endp) {
                showError (filename, lineno, "Illegal integer `%s'", arg);
                return 0;
            }
        }
        else {
            pargvalue->type.ival = 0;
        }
        pioccrfArg->value = &pargvalue->type.ival;
        break;

    case ioccrfArgDouble:
        if (arg && *arg) {
            pargvalue->type.dval = strtod (arg, &endp);
            if (*endp) {
                showError (filename, lineno, "Illegal double `%s'", arg);
                return 0;
            }
        }
        else {
            pargvalue->type.dval = 0.0;
        }
        pioccrfArg->value = &pargvalue->type.dval;
        break;

    case ioccrfArgString:
        pioccrfArg->value = arg;
        break;

    case ioccrfArgPdbbase:
        /* Argumenmt must be missing or 0 or pdbbase */
        if(!arg || !*arg || (*arg == '0') || (strcmp(arg, "pdbbase") == 0)) {
            pioccrfArg->value = pdbbase;
            break;
        }
        showError (filename, lineno, "Expecting `pdbbase' got `%s'", arg);
        return 0;

    default:
        showError (filename, lineno, "Illegal argument type %d", pioccrfArg->type);
        return 0;
    }
    return 1;
}

/*
 * The body of the command interpreter
 */
int epicsShareAPI
ioccrf (const char *pathname)
{
    FILE *fp;
    const char *filename = NULL;
    int icin, icout;
    int c, quote, inword, backslash;
    char *line = NULL;
    int lineno = 0;
    int argc;
    char **argv = NULL;
    int argvsize = 0;
    int sep;
    const char *prompt;
    const char *ifs = " \t(),";
    ioccrfFunc *pioccrfFunc;
    ioccrfFuncDef *pioccrfFuncDef;
    int arg;
    argvalue *pargvalue, *prevargvalue, *argvalueHead = NULL;
    ioccrfArg *pioccrfArg;
    
    /*
     * See if command interpreter is interactive
     */
    if (pathname == NULL) {
        const char *historySize;
        fp = stdin;
        if ((prompt = getenv ("IOCSH_PS1")) == NULL)
            prompt = "iocsh> ";
        if (((historySize = getenv ("IOCSH_HISTSIZE")) == NULL)
         && ((historySize = getenv ("HISTSIZE")) == NULL))
            historySize = "10";
        stifle_history (atoi (historySize));
        /*
         * FIXME: Could enable tab-completion of commands here
         */
        rl_bind_key ('\t', rl_insert);
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
        line = my_readline (fp, prompt);
        if (line == NULL)
            break;

        /*
         * If interactive, add non-blank lines to history
         */
        if ((fp == stdin) && *line)
            add_history (line);

        /*
         * Ignore comment lines
         */
        if (*line == '#')
            continue;

        /*
         * Break line into words
         */
        icout = icin = 0;
        inword = 0;
        quote = EOF;
        backslash = 0;
        argc = 0;
        for (;;) {
            if (argc >= argvsize) {
                char **av;
                argvsize += 50;
                av = realloc (argv, argvsize * sizeof *argv);
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

             /*
              * Look up command
              */
            pioccrfFunc = 0;
            if(pioccrfFuncHead) 
                pioccrfFunc = (ioccrfFunc *)registryFind (ioccrfID, argv[0]);
            if (!pioccrfFunc) {
                showError (filename, lineno, "Command %s not found.", argv[0]);
                continue;
            }

            /*
             * Process arguments
             */
            pioccrfFuncDef = pioccrfFunc->pioccrfFuncDef;
            pargvalue = argvalueHead;
            prevargvalue = NULL;
            for (arg = 0 ; ; arg++) {
                char *p = (arg < argc) ? argv[arg+1] : NULL;

                if (arg == pioccrfFuncDef->nargs) {
                    (*pioccrfFunc->func)(pioccrfFuncDef->arg);
                    break;
                }
                pioccrfArg = pioccrfFuncDef->arg[arg];
                if (!pargvalue) {
                    pargvalue = callocMustSucceed(1, sizeof(argvalue), "ioccrf");
                    if ( prevargvalue ) {
                        prevargvalue->next = pargvalue;
                    }
                    else {
                        argvalueHead = pargvalue;
                    }
                }
                if (!cvtArg (filename, lineno, p, pargvalue, pioccrfArg))
                    break;
                prevargvalue = pargvalue;
                pargvalue = prevargvalue->next;
            }
        }
    }
    if (fp != stdin)
        fclose (fp);
    free (line);
    free (argv);
    while (argvalueHead) {
        pargvalue = argvalueHead->next;
        free (argvalueHead);
        argvalueHead = pargvalue;
    }
    return 0;
}

/* Readline automatic completion code could go here! */
