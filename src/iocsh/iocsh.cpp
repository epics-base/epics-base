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
#include "macLib.h"
#include "epicsString.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "envDefs.h"
#include "registry.h"
#define epicsExportSharedSymbols
#include "iocsh.h"
#include "epicsReadline.h"

/*
 * File-local information
 */
struct iocshCommand {
    iocshFuncDef const    *pFuncDef;
    iocshCallFunc          func;
    struct iocshCommand   *next;
};
static struct iocshCommand *iocshCommandHead;
static char iocshCmdID[] = "iocshCmd";
struct iocshVariable {
    iocshVarDef const     *pVarDef;
    struct iocshVariable  *next;
};
static struct iocshVariable *iocshVariableHead;
static char iocshVarID[] = "iocshVar";
extern "C" { static void varCallFunc(const iocshArgBuf *); }
static epicsMutexId iocshTableMutex;
static epicsThreadOnceId iocshTableOnceId = EPICS_THREAD_ONCE_INIT;

/*
 * Set up command table mutex
 */
extern "C" {
static void iocshTableOnce (void *)
{
    iocshTableMutex = epicsMutexMustCreate ();
}
}

/*
 * Lock the table mutex
 */
static void
iocshTableLock (void)
{
    epicsThreadOnce (&iocshTableOnceId, iocshTableOnce, NULL);
    epicsMutexMustLock (iocshTableMutex);
}

/*
 * Unlock the table mutex
 */
static void
iocshTableUnlock (void)
{
    epicsThreadOnce (&iocshTableOnceId, iocshTableOnce, NULL);
    epicsMutexUnlock (iocshTableMutex);
}

/*
 * Register a command
 */
void epicsShareAPI iocshRegister (const iocshFuncDef *piocshFuncDef, iocshCallFunc func)
{
    struct iocshCommand *l, *p, *n;
    int i;

    iocshTableLock ();
    for (l = NULL, p = iocshCommandHead ; p != NULL ; l = p, p = p->next) {
        i = strcmp (piocshFuncDef->name, p->pFuncDef->name);
        if (i == 0) {
            p->pFuncDef = piocshFuncDef;
            p->func = func;
            iocshTableUnlock ();
            return;
        }
        if (i < 0)
            break;
    }
    n = (struct iocshCommand *)callocMustSucceed (1, sizeof *n, "iocshRegister");
    if (!registryAdd(iocshCmdID, piocshFuncDef->name, (void *)n)) {
        free (n);
        iocshTableUnlock ();
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
    iocshTableUnlock ();
}

/*
 * Register variable(s)
 */
void epicsShareAPI iocshRegisterVariable (const iocshVarDef *piocshVarDef)
{
    struct iocshVariable *l, *p, *n;
    int i;
    int found;
    static const iocshArg varArg0 = { "[variable",iocshArgString};
    static const iocshArg varArg1 = { "[value]]",iocshArgString};
    static const iocshArg *varArgs[2] = {&varArg0, &varArg1};
    static const iocshFuncDef varFuncDef = {"var",2,varArgs};

    iocshTableLock ();
    while ((piocshVarDef != NULL)
        && (piocshVarDef->name != NULL)
        && (*piocshVarDef->name != '\0')) {
        if (iocshVariableHead == NULL)
            iocshRegister(&varFuncDef,varCallFunc);
        found = 0;
        for (l = NULL, p = iocshVariableHead ; p != NULL ; l = p, p = p->next) {
            i = strcmp (piocshVarDef->name, p->pVarDef->name);
            if (i == 0) {
                errlogPrintf("Warning -- iocshRegisterVariable redefining %s.\n", piocshVarDef->name);
                p->pVarDef = piocshVarDef;
                found = 1;
                break;
            }
            if (i < 0)
                break;
        }
        if (!found) {
            n = (struct iocshVariable *)callocMustSucceed(1, sizeof *n, "iocshRegisterVariable");
            if (!registryAdd(iocshVarID, piocshVarDef->name, (void *)n)) {
                free(n);
                iocshTableUnlock();
                errlogPrintf("iocshRegisterVariable failed to add %s.\n", piocshVarDef->name);
                return;
            }
            if (l == NULL) {
                n->next = iocshVariableHead;
                iocshVariableHead = n;
            }
            else {
                n->next = l->next;
                l->next = n;
            }
            n->pVarDef = piocshVarDef;
        }
        piocshVarDef++;
    }
    iocshTableUnlock ();
}

/*
 * Free storage created by iocshRegister/iocshRegisterVariable
 */
void epicsShareAPI iocshFree(void) 
{
    struct iocshCommand *pc, *nc;
    struct iocshVariable *pv, *nv;

    iocshTableLock ();
    for (pc = iocshCommandHead ; pc != NULL ; ) {
        nc = pc->next;
        free (pc);
        pc = nc;
    }
    for (pv = iocshVariableHead ; pv != NULL ; ) {
        nv = pv->next;
        free (pv);
        pv = nv;
    }
    iocshTableUnlock ();
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

    case iocshArgPersistentString:
        argBuf->sval = epicsStrDup(arg);
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
    char *raw;
    char *line = NULL;
    int lineno = 0;
    int argc;
    char **argv = NULL;
    int argvCapacity = 0;
    int iarg;
    int sep;
    const char *prompt;
    const char *ifs = " \t(),";
    iocshArgBuf *argBuf = NULL;
    int argBufCapacity = 0;
    struct iocshCommand *found;
    struct iocshFuncDef const *piocshFuncDef;
    void *readlineContext;
    
    /*
     * See if command interpreter is interactive
     */
    if ((pathname == NULL) || (strcmp (pathname, "<telnet>") == 0)) {
        if ((prompt = envGetConfigParamPtr(&IOCSH_PS1)) == NULL)
            prompt = "epics> ";
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
     * Create a command-line input context
     */
    if ((readlineContext = epicsReadlineBegin(fp)) == NULL) {
        fprintf(stderr, "Can't allocate command-line object.\n");
        if (fp)
            fclose(fp);
        return -1;
    }

    /*
     * Read commands till EOF or exit
     */
    argc = 0;
    while ((raw = epicsReadline(prompt, readlineContext)) != NULL) {
        lineno++;

        /*
         * Ignore comment lines
         */
        if (*raw == '#')
            continue;

        /*
         * Expand macros
         */
        free(line);
        if ((line = macEnvExpand(raw)) == NULL)
            continue;

        /*
         * Echo commands read from scripts
         */
        if ((prompt == NULL) && *line)
            puts(line);

        /*
         * Break line into words
         */
        icout = icin = 0;
        inword = 0;
        argc = 0;
        quote = EOF;
        backslash = 0;
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
                    iocshTableLock ();
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
                    iocshTableUnlock ();
                }
                else {
                    for (iarg = 1 ; iarg < argc ; iarg++) {
                        found = (iocshCommand *)registryFind (iocshCmdID, argv[iarg]);
                        if (found == NULL) {
                            printf ("%s -- no such command.\n", argv[iarg]);
                        }
                        else {
                            piocshFuncDef = found->pFuncDef;
                            fputs (piocshFuncDef->name, stdout);
                            for (int a = 0 ; a < piocshFuncDef->nargs ; a++) {
                                const char *cp = piocshFuncDef->arg[a]->name;
                                if ((piocshFuncDef->arg[a]->type == iocshArgArgv)
                                 || (strchr (cp, ' ') == NULL)) {
                                    fprintf (stdout, " %s", cp);
                                }
                                else {
                                    fprintf (stdout, " '%s'", cp);
                                }
                            }
                            fprintf (stdout,"\n");;
                        }
                    }
                }
                continue;
            }

            /*
             * Look up command
             */
            found = (iocshCommand *)registryFind (iocshCmdID, argv[0]);
            if (!found) {
                showError (filename, lineno, "Command %s not found.", argv[0]);
                continue;
            }
            piocshFuncDef = found->pFuncDef;

            /*
             * Process arguments and call function
             */
            for (iarg = 0 ; ; iarg++) {
                if (iarg == piocshFuncDef->nargs) {
                    (*found->func)(argBuf);
                    break;
                }
                if (iarg >= argBufCapacity) {
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
                if (piocshFuncDef->arg[iarg]->type == iocshArgArgv) {
                    argBuf[iarg].aval.ac = argc-iarg;
                    argBuf[iarg].aval.av = argv+iarg;
                    (*found->func)(argBuf);
                    break;
                }
                if (!cvtArg (filename, lineno,
                                        ((iarg < argc) ? argv[iarg+1] : NULL),
                                        &argBuf[iarg], piocshFuncDef->arg[iarg]))
                    break;
            }
            if((prompt != NULL) && (strcmp(argv[0], "epicsEnvSet") == 0)) {
                const char *newPrompt;
                if ((newPrompt = envGetConfigParamPtr(&IOCSH_PS1)) != NULL)
                    prompt = newPrompt;
            }
        }
    }
    if (fp && (fp != stdin))
        fclose (fp);
    free(line);
    free (argv);
    free (argBuf);
    errlogFlush();
    epicsReadlineEnd(readlineContext);
    return 0;
}

/*
 * Internal commands
 */
static void varHandler(const iocshVarDef *v, const char *setString)
{
    switch(v->type) {
    default:
        printf("Can't handle variable %s of type %d.\n", v->name, v->type);
        return;
    case iocshArgInt: break;
    case iocshArgDouble: break;
    }
    if(setString == NULL) {
        switch(v->type) {
        default: break;
        case iocshArgInt:
            printf("%s = %d\n", v->name, *(int *)v->pval);
            break;
        case iocshArgDouble:
            printf("%s = %g\n", v->name, *(double *)v->pval);
            break;
        }
    }
    else {
        switch(v->type) {
        default: break;
        case iocshArgInt:
          {
            char *endp;
            long ltmp = strtol(setString, &endp, 0);
            if((*setString != '\0') && (*endp == '\0'))
                *(int *)v->pval = ltmp;
            else
                printf("Invalid value -- value of %s not changed.\n", v->name);
            break;
          }
        case iocshArgDouble:
          {
            char *endp;
            double dtmp = strtod(setString, &endp);
            if((*setString != '\0') && (*endp == '\0'))
                *(double *)v->pval = dtmp;
            else
                printf("Invalid value -- value of %s not changed.\n", v->name);
            break;
          }
        }
    }
}

extern "C" {

static void varCallFunc(const iocshArgBuf *args)
{
    struct iocshVariable *v;
    if(args[0].sval == NULL) {
        for (v = iocshVariableHead ; v != NULL ; v = v->next)
            varHandler(v->pVarDef, args[1].sval);
    }
    else {
        v = (iocshVariable *)registryFind(iocshVarID, args[0].sval);
        if (v == NULL) {
            printf("%s -- no such variable.\n", args[0].sval);
        }
        else {
            varHandler(v->pVarDef, args[1].sval);
        }
    }
}

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
