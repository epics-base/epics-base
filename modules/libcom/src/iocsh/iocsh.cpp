/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* iocsh.cpp */
/* Author:  Marty Kraimer Date: 27APR2000 */
/* Heavily modified by Eric Norum   Date: 03MAY2000 */
/* Adapted to C++ by Eric Norum   Date: 18DEC2000 */

#include <exception>

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "epicsMath.h"
#include "errlog.h"
#include "macLib.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "epicsStdlib.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "envDefs.h"
#include "registry.h"
#include "epicsReadline.h"
#include "cantProceed.h"
#include "iocsh.h"

extern "C" {

/*
 * Global link to pdbbase
 */
struct dbBase **iocshPpdbbase;

/*
 * File-local information
 */
struct iocshCommand {
    iocshCmdDef            def;
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
static epicsThreadOnceId iocshOnceId = EPICS_THREAD_ONCE_INIT;
static epicsThreadPrivateId iocshContextId;

/*
 * I/O redirection
 */
#define NREDIRECTS   5
struct iocshRedirect {
    const char *name;
    const char *mode;
    FILE       *fp;
    FILE       *oldFp;
    int         mustRestore;
};

/*
 * Set up module variables
 */
static void iocshOnce (void *)
{
    iocshTableMutex = epicsMutexMustCreate ();
    iocshContextId = epicsThreadPrivateCreate();
}

static void iocshInit (void)
{
    epicsThreadOnce (&iocshOnceId, iocshOnce, NULL);
}

/*
 * Lock the table mutex
 */
static void
iocshTableLock (void)
{
    iocshInit();
    epicsMutexMustLock (iocshTableMutex);
}

/*
 * Unlock the table mutex
 */
static void
iocshTableUnlock (void)
{
    epicsMutexUnlock (iocshTableMutex);
}

/*
 * Register a command
 */
void epicsStdCall iocshRegister (const iocshFuncDef *piocshFuncDef,
    iocshCallFunc func)
{
    struct iocshCommand *l, *p, *n;
    int i;

    iocshTableLock ();
    for (l = NULL, p = iocshCommandHead ; p != NULL ; l = p, p = p->next) {
        i = strcmp (piocshFuncDef->name, p->def.pFuncDef->name);
        if (i == 0) {
            p->def.pFuncDef = piocshFuncDef;
            p->def.func = func;
            iocshTableUnlock ();
            return;
        }
        if (i < 0)
            break;
    }
    n = (struct iocshCommand *) callocMustSucceed (1, sizeof *n,
        "iocshRegister");
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
    n->def.pFuncDef = piocshFuncDef;
    n->def.func = func;
    iocshTableUnlock ();
}


/*
 * Retrieves a previously registered function with the given name.
 */
const iocshCmdDef * epicsStdCall iocshFindCommand(const char *name)
{
    return (iocshCmdDef *) registryFind(iocshCmdID, name);
}

/*
 * Register the "var" command and any variable(s)
 */
static const iocshArg varCmdArg0 = { "[variable", iocshArgString};
static const iocshArg varCmdArg1 = { "[value]]", iocshArgString};
static const iocshArg *varCmdArgs[2] = {&varCmdArg0, &varCmdArg1};
static const iocshFuncDef varFuncDef = {"var", 2, varCmdArgs,
                                        "Print all, print single variable or set value to single variable\n"
                                        "  (default) - print all variables and their values"
                                        " defined in database definitions files\n"
                                        "  variable  - if only parameter print value for this variable\n"
                                        "  value     - set the value to variable\n"};

void epicsStdCall iocshRegisterVariable (const iocshVarDef *piocshVarDef)
{
    struct iocshVariable *l, *p, *n;
    int i;
    int found;

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
                if ((p->pVarDef->type != piocshVarDef->type) && (p->pVarDef->pval != piocshVarDef->pval)) {
                    errlogPrintf("Warning: iocshRegisterVariable redefining %s.\n",
                        piocshVarDef->name);
                    p->pVarDef = piocshVarDef;
                }
                found = 1;
                break;
            }
            if (i < 0)
                break;
        }
        if (!found) {
            n = (struct iocshVariable *) callocMustSucceed(1, sizeof *n,
                "iocshRegisterVariable");
            if (!registryAdd(iocshVarID, piocshVarDef->name, (void *)n)) {
                free(n);
                iocshTableUnlock();
                errlogPrintf("iocshRegisterVariable failed to add %s.\n",
                    piocshVarDef->name);
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
 * Retrieves a previously registered variable with the given name.
 */
const iocshVarDef * epicsStdCall iocshFindVariable(const char *name)
{
    struct iocshVariable *temp = (iocshVariable *) registryFind(iocshVarID, name);
    return temp ? temp->pVarDef : 0;
}

/*
 * Free storage created by iocshRegister/iocshRegisterVariable
 */
void epicsStdCall iocshFree(void) 
{
    struct iocshCommand *pc;
    struct iocshVariable *pv;

    iocshTableLock ();
    for (pc = iocshCommandHead ; pc != NULL ; ) {
        struct iocshCommand * nc = pc->next;
        free (pc);
        pc = nc;
    }
    for (pv = iocshVariableHead ; pv != NULL ; ) {
        struct iocshVariable *nv = pv->next;
        free (pv);
        pv = nv;
    }
    iocshCommandHead = NULL;
    iocshVariableHead = NULL;
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
        fprintf(epicsGetStderr(), "%s line %d: ", filename, lineno);
    vfprintf (epicsGetStderr(), msg, ap);
    fputc ('\n', epicsGetStderr());
    va_end (ap);
}

static int
cvtArg (const char *filename, int lineno, char *arg, iocshArgBuf *argBuf,
    const iocshArg *piocshArg)
{
    char *endp;

    switch (piocshArg->type) {
    case iocshArgInt:
        if (arg && *arg) {
            errno = 0;
            argBuf->ival = strtol (arg, &endp, 0);
            if (errno == ERANGE) {
                errno = 0;
                argBuf->ival = strtoul (arg, &endp, 0);
                if (errno == ERANGE) {
                    showError(filename, lineno, "Integer '%s' out of range",
                        arg);
                    return 0;
                }
            }
            if (*endp) {
                showError(filename, lineno, "Illegal integer '%s'", arg);
                return 0;
            }
        }
        else {
            argBuf->ival = 0;
        }
        break;

    case iocshArgDouble:
        if (arg && *arg) {
            argBuf->dval = epicsStrtod (arg, &endp);
            if (*endp) {
                showError(filename, lineno, "Illegal double '%s'", arg);
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
        if (arg != NULL) {
            argBuf->sval = (char *) malloc(strlen(arg) + 1);
            if (argBuf->sval == NULL) {
                showError(filename, lineno, "Out of memory");
                return 0;
            }
            strcpy(argBuf->sval, arg);
        } else {
          argBuf->sval = NULL;
        }
        break;

    case iocshArgPdbbase:
        /* Argument must be missing or 0 or pdbbase */
        if(!arg || !*arg || (*arg == '0') || (strcmp(arg, "pdbbase") == 0)) {
            if(!iocshPpdbbase || !*iocshPpdbbase) {
                showError(filename, lineno, "pdbbase not present");
                return 0;
            }
            argBuf->vval = *iocshPpdbbase;
            break;
        }
        showError(filename, lineno, "Expecting 'pdbbase' got '%s'", arg);
        return 0;

    default:
        showError(filename, lineno, "Illegal argument type %d",
            piocshArg->type);
        return 0;
    }
    return 1;
}

/*
 * Open redirected I/O
 */
static int
openRedirect(const char *filename, int lineno, struct iocshRedirect *redirect)
{
    int i;

    for (i = 0 ; i < NREDIRECTS ; i++, redirect++) {
        if (redirect->name != NULL) {
            redirect->fp = fopen(redirect->name, redirect->mode);
            if (redirect->fp == NULL) {
                showError(filename, lineno, "Can't open \"%s\": %s.",
                                            redirect->name, strerror(errno));
                redirect->name = NULL;
                while (i--) {
                    redirect--;
                    if (redirect->fp) {
                        fclose(redirect->fp);
                        redirect->fp = NULL;
                    }
                    redirect->name = NULL;
                }
                return -1;
            }
            redirect->mustRestore = 0;
        }
    }
    return 0;
}

/*
 * Start I/O redirection
 */
static void
startRedirect(const char * /*filename*/, int /*lineno*/,
              struct iocshRedirect *redirect)
{
    int i;

    for (i = 0 ; i < NREDIRECTS ; i++, redirect++) {
        if (redirect->fp != NULL) {
            switch(i) {
            case 0:
                redirect->oldFp = epicsGetThreadStdin();
                epicsSetThreadStdin(redirect->fp);
                redirect->mustRestore = 1;
                break;
            case 1:
                redirect->oldFp = epicsGetThreadStdout();
                epicsSetThreadStdout(redirect->fp);
                redirect->mustRestore = 1;
                break;
            case 2:
                redirect->oldFp = epicsGetThreadStderr();
                epicsSetThreadStderr(redirect->fp);
                redirect->mustRestore = 1;
                break;
            }
        }
    }
}

/*
 * Finish up I/O redirection
 */
static void
stopRedirect(const char *filename, int lineno, struct iocshRedirect *redirect)
{
    int i;

    for (i = 0 ; i < NREDIRECTS ; i++, redirect++) {
        if (redirect->fp != NULL) {
            if (fclose(redirect->fp) != 0)
                showError(filename, lineno, "Error closing \"%s\": %s.",
                                            redirect->name, strerror(errno));
            redirect->fp = NULL;
            if (redirect->mustRestore) {
                switch(i) {
                case 0: epicsSetThreadStdin(redirect->oldFp);  break;
                case 1: epicsSetThreadStdout(redirect->oldFp); break;
                case 2: epicsSetThreadStderr(redirect->oldFp); break;
                }
            }
        }
        redirect->name = NULL;
    }
}

/*
 * "help" command
 */
static const iocshArg helpArg0 = { "[command ...]",iocshArgArgv};
static const iocshArg *helpArgs[1] = {&helpArg0};
static const iocshFuncDef helpFuncDef =
    {"help",1,helpArgs,
    "With no arguments, list available command names.\n"
    "With arguments, list arguments and usage for command(s).\n"
    "Command names may contain wildcards\n"};
static void helpCallFunc(const iocshArgBuf *args)
{
    int argc = args[0].aval.ac;
    const char * const * argv = args[0].aval.av;
    struct iocshFuncDef const *piocshFuncDef;
    struct iocshCommand *pcmd;

    if (argc == 1) {
        int l, col = 0;

        iocshTableLock ();
        for (pcmd = iocshCommandHead ; pcmd != NULL ; pcmd = pcmd->next) {
            piocshFuncDef = pcmd->def.pFuncDef;
            l = strlen (piocshFuncDef->name);
            if ((l + col) >= 79) {
                fputc('\n', epicsGetStdout());
                col = 0;
            }
            fputs(piocshFuncDef->name, epicsGetStdout());
            col += l;
            if (col >= 64) {
                fputc('\n', epicsGetStdout());
                col = 0;
            }
            else {
                do {
                    fputc(' ', epicsGetStdout());
                    col++;
                } while ((col % 16) != 0);
            }
        }
        if (col)
            fputc('\n', epicsGetStdout());
        iocshTableUnlock ();

        fprintf(epicsGetStdout(),
                "\n"
                "Type 'help <command>' to see the arguments of <command>.  eg. 'help db*'\n");
    }
    else {
        for (int iarg = 1 ; iarg < argc ; iarg++) {
            for (pcmd = iocshCommandHead ; pcmd != NULL ; pcmd = pcmd->next) {
                piocshFuncDef = pcmd->def.pFuncDef;
                if (epicsStrGlobMatch(piocshFuncDef->name, argv[iarg]) != 0) {
                    if(piocshFuncDef->usage) {
                        fputs("\nUsage: ", epicsGetStdout());
                    }
                    fprintf(epicsGetStdout(),
                            ANSI_BOLD("%s"),
                            piocshFuncDef->name);

                    for (int a = 0 ; a < piocshFuncDef->nargs ; a++) {
                        const char *cp = piocshFuncDef->arg[a]->name;
                        if ((piocshFuncDef->arg[a]->type == iocshArgArgv)
                         || (strchr (cp, ' ') == NULL)) {
                            fprintf(epicsGetStdout(), " %s", cp);
                        }
                        else {
                            fprintf(epicsGetStdout(), " '%s'", cp);
                        }
                    }
                    fprintf(epicsGetStdout(),"\n");;
                    if(piocshFuncDef->usage) {
                        fprintf(epicsGetStdout(), "\n%s", piocshFuncDef->usage);
                    }
                }
            }
        }
    }
}

typedef enum {
    Continue,
    Break,
    Halt
} OnError;

// per call to iocshBody()
struct iocshScope {
    iocshScope *outer;
    OnError onerr;
    double timeout;
    bool errored;
    bool interactive;
    iocshScope() :outer(0), onerr(Continue), timeout(0.0), errored(false), interactive(false) {}
};

// per thread executing iocshBody()
struct iocshContext {
    MAC_HANDLE *handle;
    iocshScope *scope;
};

int iocshSetError(int err)
{
    iocshContext *ctxt;
    if (err && iocshContextId) {
        ctxt = (iocshContext *) epicsThreadPrivateGet(iocshContextId);

        if(ctxt && ctxt->scope) ctxt->scope->errored = 1;
    }
    return err;
}

/*
 * The body of the command interpreter
 */
static int
iocshBody (const char *pathname, const char *commandLine, const char *macros)
{
    FILE *fp = NULL;
    const char *filename = NULL;
    int icin, icout;
    char c;
    int quote, inword, backslash;
    const char *raw = NULL;;
    char *line = NULL;
    int lineno = 0;
    int argc;
    char **argv = NULL;
    int argvCapacity = 0;
    struct iocshRedirect *redirects = NULL;
    struct iocshRedirect *redirect = NULL;
    int sep;
    const char *prompt = NULL;
    const char *ifs = " \t(),\r";
    iocshArgBuf *argBuf = NULL;
    int argBufCapacity = 0;
    struct iocshCommand *found;
    void *readlineContext = NULL;
    int wasOkToBlock;
    static const char * pairs[] = {"", "environ", NULL, NULL};
    iocshScope scope;
    iocshContext *context;
    char ** defines = NULL;
    int ret = 0;

    iocshInit();

    /*
     * See if command interpreter is interactive
     */
    if (commandLine == NULL) {
        if ((pathname == NULL) || (strcmp (pathname, "<telnet>") == 0)) {
            if ((prompt = envGetConfigParamPtr(&IOCSH_PS1)) == NULL) {
                prompt = "epics> ";
            }
            scope.interactive = true;
        }
        else {
            fp = fopen (pathname, "r");
            if (fp == NULL) {
                fprintf(epicsGetStderr(), "Can't open %s: %s\n", pathname,
                    strerror (errno));
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
            fprintf(epicsGetStderr(), "Can't allocate command-line object.\n");
            if (fp)
                fclose(fp);
            return -1;
        }

    } else {
        // use of iocshCmd() implies "on error break"
        scope.onerr = Break;
    }

    /*
     * Set up redirection
     */
    redirects = (struct iocshRedirect *)calloc(NREDIRECTS, sizeof *redirects);
    if (redirects == NULL) {
        fprintf(epicsGetStderr(), "Out of memory!\n");
        return -1;
    }

    /*
     * Parse macro definitions, this check occurs before creating the
     * macro handle to simplify cleanup.
     */

    if (macros) {
        if (macParseDefns(NULL, macros, &defines) < 0) {
            free(redirects);
            return -1;
        }
    }

    // Check for existing context or construct a new one.
    context = (iocshContext *) epicsThreadPrivateGet(iocshContextId);

    if (!context) {
        context = (iocshContext*)calloc(1, sizeof(*context));
        if (!context || macCreateHandle(&context->handle, pairs)) {
            errlogMessage("iocsh: macCreateHandle failed.");
            free(redirects);
            free(context);
            return -1;
        }

        epicsThreadPrivateSet(iocshContextId, (void *) context);
    }
    MAC_HANDLE *handle = context->handle;

    scope.outer = context->scope;
    context->scope = &scope;

    macPushScope(handle);
    macInstallMacros(handle, defines);

    wasOkToBlock = epicsThreadIsOkToBlock();
    epicsThreadSetOkToBlock(1);

    /*
     * Read commands till EOF or exit
     */
    for (;;) {
        if(!scope.interactive && scope.errored) {
            if(scope.onerr==Continue) {
                /* do nothing */

            } else if(scope.onerr==Break) {
                ret = -1;
                fprintf(epicsGetStderr(), "iocsh Error: Break\n" );
                break;

            } else if(scope.onerr==Halt) {
                ret = -1;
                if(scope.timeout<=0.0 || isinf(scope.timeout)) {
                    fprintf(epicsGetStderr(), "iocsh Error: Halt\n" );
                    epicsThreadSuspendSelf();
                    break;

                } else {
                    fprintf(epicsGetStderr(), "iocsh Error: Waiting %.1f sec ...\n", scope.timeout);
                    epicsThreadSleep(scope.timeout);
                }
            }
        }

        /*
         * Read a line
         */
        if (commandLine) {
            if (raw != NULL)
                break;
            raw = commandLine;
        }
        else {
            if ((raw = epicsReadline(prompt, readlineContext)) == NULL)
                break;
        }
        lineno++;

        /*
         * Skip leading white-space
         */
        icin = 0;
        while ((c = raw[icin]) && isspace(c)) {
            icin++;
        }

        /*
         * Ignore comment lines other than to echo
         * them if they came from a script (disable echoing
         * with '#-').  This avoids macLib errors from comments.
         */
        if (c == '#') {
            if ((prompt == NULL) && (commandLine == NULL))
                if (raw[icin + 1] != '-')
                    puts(raw);
            continue;
        }

        /*
         * Expand macros
         */
        free(line);
        if ((line = macDefExpand(raw, handle)) == NULL) {
            scope.errored = true;
            continue;
        }

        /*
         * Skip leading white-space coming from a macro
         */
        while ((c = line[icin]) && isspace(c)) {
            icin++;
        }

        /*
         * Echo non-empty lines read from a script.
         * Comments delineated with '#-' aren't echoed.
         */
        if ((prompt == NULL) && *line && (commandLine == NULL)) {
            if ((c != '#') || (line[icin + 1] != '-')) {
                puts(line);
            }
        }

        /*
         * Ignore lines that became a comment or empty after macro expansion
         */
        if (!c || c == '#')
            continue;

        /*
         * Break line into words
         */
        icout = 0;
        inword = 0;
        argc = 0;
        quote = EOF;
        backslash = 0;
        redirect = NULL;
        for (;;) {
            if (argc >= argvCapacity) {
                int newCapacity = argvCapacity + 20;
                char **newv = (char **)realloc (argv, newCapacity * sizeof *argv);
                if (newv == NULL) {
                    fprintf (epicsGetStderr(), "Out of memory!\n");
                    argc = -1;
                    scope.errored = true;
                    break;
                }
                argv = newv;
                argvCapacity = newCapacity;
            }
            c = line[icin++];
            if (c == '\0')
                break;
            if ((quote == EOF) && !backslash && (strchr (ifs, c)))
                sep = 1;
            else
                sep = 0;
            if ((quote == EOF) && !backslash) {
                int redirectFd = 1;
                if (c == '\\') {
                    backslash = 1;
                    continue;
                }
                if (c == '<') {
                    if (redirect != NULL) {
                        break;
                    }
                    redirect = &redirects[0];
                    sep = 1;
                    redirect->mode = "r";
                }
                if ((c >= '1') && (c <= '9') && (line[icin] == '>')) {
                    redirectFd = c - '0';
                    c = '>';
                    icin++;
                }
                if (c == '>') {
                    if (redirect != NULL)
                        break;
                    if (redirectFd >= NREDIRECTS) {
                        redirect = &redirects[1];
                        break;
                    }
                    redirect = &redirects[redirectFd];
                    sep = 1;
                    if (line[icin] == '>') {
                        icin++;
                        redirect->mode = "a";
                    }
                    else {
                        redirect->mode = "w";
                    }
                }
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
                    if (((c == '"') || (c == '\'')) && !backslash) {
                        quote = c;
                    }
                    if (redirect != NULL) {
                        if (redirect->name != NULL) {
                            argc = -1;
                            break;
                        }
                        redirect->name = line + icout;
                        redirect = NULL;
                    }
                    else {
                        argv[argc++] = line + icout;
                    }
                    if (quote == EOF)
                        line[icout++] = c;
                    inword = 1;
                }
            }
            backslash = 0;
        }
        if (redirect != NULL) {
            showError(filename, lineno, "Illegal redirection.");
            scope.errored = true;
            continue;
        }
        if (argc < 0) {
            break;
        }
        if (quote != EOF) {
            showError(filename, lineno, "Unbalanced quote.");
            scope.errored = true;
            continue;
        }
        if (backslash) {
            showError(filename, lineno, "Trailing backslash.");
            scope.errored = true;
            continue;
        }
        if (inword)
            line[icout++] = '\0';
        argv[argc] = NULL;

        /*
         * Special case -- Redirected input but no command
         * Treat as if 'iocsh filename'.
         */
        if ((argc == 0) && (redirects[0].name != NULL)) {
            const char *commandFile = redirects[0].name;
            redirects[0].name = NULL;
            if (openRedirect(filename, lineno, redirects) < 0)
                continue;
            startRedirect(filename, lineno, redirects);
            if(iocshBody(commandFile, NULL, macros))
                scope.errored = true;
            stopRedirect(filename, lineno, redirects);
            continue;
        }

        /*
         * Special command?
         */
        if ((argc > 0) && (strcmp(argv[0], "exit") == 0))
            break;

        /*
         * Set up redirection
         */
        if ((openRedirect(filename, lineno, redirects) == 0) && (argc > 0)) {
            // error unless a function is actually called.
            // handles command not-found and arg parsing errors.
            scope.errored = true;
            /*
             * Look up command
             */
            found = (iocshCommand *)registryFind (iocshCmdID, argv[0]);
            if (found) {
                /*
                 * Process arguments and call function
                 */
                struct iocshFuncDef const *piocshFuncDef = found->def.pFuncDef;
                for (int iarg = 0 ; ; ) {
                    if (iarg == piocshFuncDef->nargs) {
                        startRedirect(filename, lineno, redirects);
                        /* execute */
                        scope.errored = false;
                        try {
                            (*found->def.func)(argBuf);
                        } catch(std::exception& e){
                            fprintf(epicsGetStderr(), "c++ error: %s\n", e.what());
                            scope.errored = true;
                        } catch(...) {
                            fprintf(epicsGetStderr(), "c++ error unknown\n");
                            scope.errored = true;
                        }
                        break;
                    }
                    if (iarg >= argBufCapacity) {
                        int newCapacity = argBufCapacity + 20;
                        void *newBuf = realloc(argBuf, newCapacity * sizeof *argBuf);
                        if (newBuf == NULL) {
                            fprintf (epicsGetStderr(), "Out of memory!\n");
                            break;
                        }
                        argBuf = (iocshArgBuf *) newBuf;
                        argBufCapacity = newCapacity;
                    }
                    if (piocshFuncDef->arg[iarg]->type == iocshArgArgv) {
                        argBuf[iarg].aval.ac = argc-iarg;
                        argBuf[iarg].aval.av = argv+iarg;
                        iarg = piocshFuncDef->nargs;
                    }
                    else {
                        if (!cvtArg (filename, lineno,
                                ((iarg < argc) ? argv[iarg+1] : NULL),
                                &argBuf[iarg], piocshFuncDef->arg[iarg]))
                            break;
                        iarg++;
                    }
                }
                if ((prompt != NULL) && (strcmp(argv[0], "epicsEnvSet") == 0)) {
                    const char *newPrompt;
                    if ((newPrompt = envGetConfigParamPtr(&IOCSH_PS1)) != NULL)
                        prompt = newPrompt;
                }
            }
            else {
                showError(filename, lineno, "Command %s not found.", argv[0]);
            }
        }
        stopRedirect(filename, lineno, redirects);
    }
    macPopScope(handle);

    if (!scope.outer) {
        macDeleteHandle(handle);
        free(context);
        epicsThreadPrivateSet(iocshContextId, NULL);
    } else {
        context->scope = scope.outer;
    }
    if (fp && (fp != stdin))
        fclose (fp);
    if (redirects != NULL) {
        stopRedirect(filename, lineno, redirects);
        free (redirects);
    }
    free(line);
    free (argv);
    free (argBuf);
    errlogFlush();
    if (readlineContext)
        epicsReadlineEnd(readlineContext);
    epicsThreadSetOkToBlock(wasOkToBlock);
    return ret;
}

/*
 * External access to the command interpreter
 */
int epicsStdCall
iocsh (const char *pathname)
{
    return iocshLoad(pathname, NULL);
}

int epicsStdCall
iocshCmd (const char *cmd)
{
    return iocshRun(cmd, NULL);
}

int epicsStdCall
iocshLoad(const char *pathname, const char *macros)
{
    if (pathname)
        epicsEnvSet("IOCSH_STARTUP_SCRIPT", pathname);
    return iocshBody(pathname, NULL, macros);
}

int epicsStdCall
iocshRun(const char *cmd, const char *macros)
{
    if (cmd == NULL)
        return 0;
    return iocshBody(NULL, cmd, macros);
}

/*
 * Needed to work around the necessary limitations of macLib and
 * environment variables. In every other case of macro expansion
 * it is the expected outcome that defined macros override any
 * environment variables.
 *
 * iocshLoad/Run turn this on its head as it is very likely that
 * an epicsEnvSet command may be run within the context of their
 * calls. Thus, it would be expected that the new value would be
 * returned in any future macro expansion.
 *
 * To do so, the epicsEnvSet command needs to be able to access
 * and update the shared MAC_HANDLE that the iocsh uses. Which is
 * what this function is provided for.
 */
void epicsStdCall
iocshEnvClear(const char *name)
{
    iocshContext *context;

    if (iocshContextId) {
        context = (iocshContext *) epicsThreadPrivateGet(iocshContextId);

        if (context != NULL) {
            macPutValue(context->handle, name, NULL);
        }
    }
}

/*
 * Internal commands
 */
static void varHandler(const iocshVarDef *v, const char *setString)
{
    switch(v->type) {
    default:
        fprintf(epicsGetStderr(), "Can't handle variable %s of type %d.\n",
            v->name, v->type);
        return;
    case iocshArgInt: break;
    case iocshArgDouble: break;
    }
    if(setString == NULL) {
        switch(v->type) {
        default: break;
        case iocshArgInt:
            fprintf(epicsGetStdout(), "%s = %d\n", v->name, *(int *)v->pval);
            break;
        case iocshArgDouble:
            fprintf(epicsGetStdout(), "%s = %g\n", v->name, *(double *)v->pval);
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
                fprintf(epicsGetStderr(),
                    "Invalid integer value. Var %s not changed.\n", v->name);
            break;
          }
        case iocshArgDouble:
          {
            char *endp;
            double dtmp = epicsStrtod(setString, &endp);
            if((*setString != '\0') && (*endp == '\0'))
                *(double *)v->pval = dtmp;
            else
                fprintf(epicsGetStderr(),
                    "Invalid double value. Var %s not changed.\n", v->name);
            break;
          }
        }
    }
}

static void varCallFunc(const iocshArgBuf *args)
{
    struct iocshVariable *v;
    const char *name = args[0].sval;
    const char *value = args[1].sval;

    if (!value) {
        int found = 0;
        for (v = iocshVariableHead ; v != NULL ; v = v->next)
            if (!name || epicsStrGlobMatch(v->pVarDef->name, name) != 0) {
                varHandler(v->pVarDef, NULL);
                found = 1;
            }
        if (!found && name != NULL)
            fprintf(epicsGetStderr(), "No var matching %s found.\n", name);
    }
    else {
        v = (iocshVariable *)registryFind(iocshVarID, args[0].sval);
        if (v == NULL) {
            fprintf(epicsGetStderr(), "Var %s not found.\n", name);
        }
        else {
            varHandler(v->pVarDef, value);
        }
    }
}

/* iocshCmd */
static const iocshArg iocshCmdArg0 = { "command",iocshArgString};
static const iocshArg *iocshCmdArgs[1] = {&iocshCmdArg0};
static const iocshFuncDef iocshCmdFuncDef = {"iocshCmd",1,iocshCmdArgs,
                                             "Takes a single IOC shell command and executes it\n"
                                             "  * This function is most useful to execute a single IOC shell command\n"
                                             "    from vxWorks or RTEMS startup script (or command line)\n"};
static void iocshCmdCallFunc(const iocshArgBuf *args)
{
    iocshCmd(args[0].sval);
}

/* iocshLoad */
static const iocshArg iocshLoadArg0 = { "pathname",iocshArgString};
static const iocshArg iocshLoadArg1 = { "macros", iocshArgString};
static const iocshArg *iocshLoadArgs[2] = {&iocshLoadArg0, &iocshLoadArg1};
static const iocshFuncDef iocshLoadFuncDef = {"iocshLoad",2,iocshLoadArgs,
                                              "Execute IOC shell commands provided in file from first parameter\n"
                                              "  * (optional) replace macros within the file with provided values\n"};
static void iocshLoadCallFunc(const iocshArgBuf *args)
{
    iocshLoad(args[0].sval, args[1].sval);
}

/* iocshRun */
static const iocshArg iocshRunArg0 = { "command",iocshArgString};
static const iocshArg iocshRunArg1 = { "macros", iocshArgString};
static const iocshArg *iocshRunArgs[2] = {&iocshRunArg0, &iocshRunArg1};
static const iocshFuncDef iocshRunFuncDef = {"iocshRun",2,iocshRunArgs,
                                             "Takes a single IOC shell command, replaces macros and executes it\n"
                                             "  * This function is most useful to execute a single IOC shell command\n"
                                             "    from vxWorks or RTEMS startup script (or command line)\n"};
static void iocshRunCallFunc(const iocshArgBuf *args)
{
    iocshRun(args[0].sval, args[1].sval);
}

/* on */
static const iocshArg onArg0 = { "'error' 'continue' | 'break' | 'wait' [value] | 'halt'", iocshArgArgv };
static const iocshArg *onArgs[1] = {&onArg0};
static const iocshFuncDef onFuncDef = {"on", 1, onArgs,
                                       "Change IOC shell error handling.\n"
                                       "  continue (default) - Ignores error and continue with next commands.\n"
                                       "  break - Return to caller without executing futher commands.\n"
                                       "  halt - Suspend process.\n"
                                       "  wait - stall process for [value] seconds, the continue.\n"};
static void onCallFunc(const iocshArgBuf *args)
{
    iocshContext *context = (iocshContext *) epicsThreadPrivateGet(iocshContextId);

#define USAGE() fprintf(epicsGetStderr(), "Usage: on error [continue | break | halt | wait <delay>]\n")

    if(!context || !context->scope) {
        // we are not called through iocshBody()...

    } else if(args->aval.ac<3 || strcmp(args->aval.av[1], "error")!=0) {
        USAGE();

    } else if(context->scope->interactive) {
        fprintf(epicsGetStderr(), "Interactive shell ignores  on error ...\n");

    } else {
        // don't fault on previous, ignored, errors
        context->scope->errored = false;

        if(strcmp(args->aval.av[2], "continue")==0) {
            context->scope->onerr = Continue;

        } else if(strcmp(args->aval.av[2], "break")==0) {
            context->scope->onerr = Break;

        } else if(strcmp(args->aval.av[2], "halt")==0) {
            context->scope->onerr = Halt;
            context->scope->timeout = 0.0;

        } else if(strcmp(args->aval.av[2], "wait")==0) {
            context->scope->onerr = Halt;
            if(args->aval.ac<=3) {
                USAGE();
            } else if(epicsParseDouble(args->aval.av[3], &context->scope->timeout, NULL)) {
                context->scope->timeout = 5.0;
            } else {
                USAGE();
                fprintf(epicsGetStderr(), "Unable to parse 'on error wait' time %s\n", args->aval.av[3]);
            }

        } else {
            fprintf(epicsGetStderr(), "Usage: on error [continue | break | halt | wait <delay>]\n");
            context->scope->errored = true;
        }
    }

#undef USAGE
}

/*
 * Dummy internal commands -- register and install in command table
 * so they show up in the help display
 */

/* comment */
static const iocshArg commentArg0 = { "newline-terminated comment",
    iocshArgArgv};
static const iocshArg *commentArgs[1] = {&commentArg0};
static const iocshFuncDef commentFuncDef = {"#",1,commentArgs};
static void commentCallFunc(const iocshArgBuf *)
{
}

/* exit */
static const iocshFuncDef exitFuncDef =
    {"exit",0,0,
     "Return to caller.  IOCs exit() from process.\n"};
static void exitCallFunc(const iocshArgBuf *)
{
}

static void localRegister (void)
{
    iocshRegister(&commentFuncDef,commentCallFunc);
    iocshRegister(&exitFuncDef,exitCallFunc);
    iocshRegister(&helpFuncDef,helpCallFunc);
    iocshRegister(&iocshCmdFuncDef,iocshCmdCallFunc);
    iocshRegister(&iocshLoadFuncDef,iocshLoadCallFunc);
    iocshRegister(&iocshRunFuncDef,iocshRunCallFunc);
    iocshRegister(&onFuncDef, onCallFunc);
}

} /* extern "C" */

/*
 * Register local commands on application startup
 */
class IocshRegister {
  public:
    IocshRegister() { localRegister(); }
};
static IocshRegister iocshRegisterObj;
