/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocsh.cpp */
/* Author:  Marty Kraimer Date: 27APR2000 */
/* Heavily modified by Eric Norum   Date: 03MAY2000 */
/* Adapted to C++ by Eric Norum   Date: 18DEC2000 */

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#define epicsExportSharedSymbols
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
epicsShareDef struct dbBase **iocshPpdbbase;

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
static epicsThreadPrivateId iocshMacroHandleId;

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
    iocshMacroHandleId = epicsThreadPrivateCreate();
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
void epicsShareAPI iocshRegister (const iocshFuncDef *piocshFuncDef,
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
const iocshCmdDef * epicsShareAPI iocshFindCommand(const char *name)
{
    return (iocshCmdDef *) registryFind(iocshCmdID, name);
}

/*
 * Register the "var" command and any variable(s)
 */
static const iocshArg varCmdArg0 = { "[variable", iocshArgString};
static const iocshArg varCmdArg1 = { "[value]]", iocshArgString};
static const iocshArg *varCmdArgs[2] = {&varCmdArg0, &varCmdArg1};
static const iocshFuncDef varFuncDef = {"var", 2, varCmdArgs};

void epicsShareAPI iocshRegisterVariable (const iocshVarDef *piocshVarDef)
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
                errlogPrintf("Warning: iocshRegisterVariable redefining %s.\n",
                    piocshVarDef->name);
                p->pVarDef = piocshVarDef;
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
const iocshVarDef * epicsShareAPI iocshFindVariable(const char *name)
{
    struct iocshVariable *temp = (iocshVariable *) registryFind(iocshVarID, name);
    return temp->pVarDef; 
}

/*
 * Free storage created by iocshRegister/iocshRegisterVariable
 */
void epicsShareAPI iocshFree(void) 
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
        argBuf->sval = (char *) malloc(strlen(arg) + 1);
        if (argBuf->sval == NULL) {
            showError(filename, lineno, "Out of memory");
            return 0;
        }
        strcpy(argBuf->sval, arg);
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
    {"help",1,helpArgs};
static void helpCallFunc(const iocshArgBuf *args)
{
    int argc = args[0].aval.ac;
    const char * const * argv = args[0].aval.av;
    struct iocshFuncDef const *piocshFuncDef;
    struct iocshCommand *pcmd;

    if (argc == 1) {
        int l, col = 0;

        fprintf(epicsGetStdout(),
            "Type 'help <command>' to see the arguments of <command>.\n");
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
    }
    else {
        for (int iarg = 1 ; iarg < argc ; iarg++) {
            for (pcmd = iocshCommandHead ; pcmd != NULL ; pcmd = pcmd->next) {
                piocshFuncDef = pcmd->def.pFuncDef;
                if (epicsStrGlobMatch(piocshFuncDef->name, argv[iarg]) != 0) {
                    fputs(piocshFuncDef->name, epicsGetStdout());
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
                }
            }
        }
    }
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
    MAC_HANDLE *handle;
    char ** defines = NULL;
    
    iocshInit();

    /*
     * See if command interpreter is interactive
     */
    if (commandLine == NULL) {
        if ((pathname == NULL) || (strcmp (pathname, "<telnet>") == 0)) {
            if ((prompt = envGetConfigParamPtr(&IOCSH_PS1)) == NULL)
                prompt = "epics> ";
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
    
    /*
     * Check for existing macro context or construct a new one.
     */
    handle = (MAC_HANDLE *) epicsThreadPrivateGet(iocshMacroHandleId);
    
    if (handle == NULL) {
        if (macCreateHandle(&handle, pairs)) {
            errlogMessage("iocsh: macCreateHandle failed.");
            free(redirects);
            return -1;
        }
        
        epicsThreadPrivateSet(iocshMacroHandleId, (void *) handle);
    }
    
    macPushScope(handle);
    macInstallMacros(handle, defines);
    
    /*
     * Read commands till EOF or exit
     */
    argc = 0;
    wasOkToBlock = epicsThreadIsOkToBlock();
    epicsThreadSetOkToBlock(1);
    for (;;) {
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
        if ((line = macDefExpand(raw, handle)) == NULL)
            continue;

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
        if ((prompt == NULL) && *line && (commandLine == NULL))
            if ((c != '#') || (line[icin + 1] != '-'))
                puts(line);

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
                char **av;
                argvCapacity += 50;
                av = (char **)realloc (argv, argvCapacity * sizeof *argv);
                if (av == NULL) {
                    fprintf (epicsGetStderr(), "Out of memory!\n");
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
                    if (((c == '"') || (c == '\'')) && !backslash)
                        quote = c;
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
            continue;
        }
        if (argc < 0)
            break;
        if (quote != EOF) {
            showError(filename, lineno, "Unbalanced quote.");
            continue;
        }
        if (backslash) {
            showError(filename, lineno, "Trailing backslash.");
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
            iocshBody(commandFile, NULL, macros);
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
                        (*found->def.func)(argBuf);
                        break;
                    }
                    if (iarg >= argBufCapacity) {
                        void *np;

                        argBufCapacity += 20;
                        np = realloc (argBuf, argBufCapacity * sizeof *argBuf);
                        if (np == NULL) {
                            fprintf (epicsGetStderr(), "Out of memory!\n");
                            argBufCapacity -= 20;
                            break;
                        }
                        argBuf = (iocshArgBuf *)np;
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
    
    if (handle->level == 0) {
        macDeleteHandle(handle);
        epicsThreadPrivateSet(iocshMacroHandleId, NULL);
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
    epicsThreadSetOkToBlock( wasOkToBlock);
    return 0;
}

/*
 * External access to the command interpreter
 */
int epicsShareAPI
iocsh (const char *pathname)
{
    return iocshLoad(pathname, NULL);
}

int epicsShareAPI
iocshCmd (const char *cmd)
{
    return iocshRun(cmd, NULL);
}

int epicsShareAPI
iocshLoad(const char *pathname, const char *macros)
{
    if (pathname)
        epicsEnvSet("IOCSH_STARTUP_SCRIPT", pathname);
    return iocshBody(pathname, NULL, macros);
}

int epicsShareAPI
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
void epicsShareAPI
iocshEnvClear(const char *name)
{
    MAC_HANDLE *handle;
    
    if (iocshMacroHandleId) {
        handle = (MAC_HANDLE *) epicsThreadPrivateGet(iocshMacroHandleId);
    
        if (handle != NULL) {
            macPutValue(handle, name, NULL);
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
    if(args[0].sval == NULL) {
        for (v = iocshVariableHead ; v != NULL ; v = v->next)
            varHandler(v->pVarDef, args[1].sval);
    }
    else {
        v = (iocshVariable *)registryFind(iocshVarID, args[0].sval);
        if (v == NULL) {
            fprintf(epicsGetStderr(), "Var %s not found.\n", args[0].sval);
        }
        else {
            varHandler(v->pVarDef, args[1].sval);
        }
    }
}

/* iocshCmd */
static const iocshArg iocshCmdArg0 = { "command",iocshArgString};
static const iocshArg *iocshCmdArgs[1] = {&iocshCmdArg0};
static const iocshFuncDef iocshCmdFuncDef = {"iocshCmd",1,iocshCmdArgs};
static void iocshCmdCallFunc(const iocshArgBuf *args)
{
    iocshCmd(args[0].sval);
}

/* iocshLoad */
static const iocshArg iocshLoadArg0 = { "pathname",iocshArgString};
static const iocshArg iocshLoadArg1 = { "macros", iocshArgString};
static const iocshArg *iocshLoadArgs[2] = {&iocshLoadArg0, &iocshLoadArg1};
static const iocshFuncDef iocshLoadFuncDef = {"iocshLoad",2,iocshLoadArgs};
static void iocshLoadCallFunc(const iocshArgBuf *args)
{
    iocshLoad(args[0].sval, args[1].sval);
}

/* iocshRun */
static const iocshArg iocshRunArg0 = { "command",iocshArgString};
static const iocshArg iocshRunArg1 = { "macros", iocshArgString};
static const iocshArg *iocshRunArgs[2] = {&iocshRunArg0, &iocshRunArg1};
static const iocshFuncDef iocshRunFuncDef = {"iocshRun",2,iocshRunArgs};
static void iocshRunCallFunc(const iocshArgBuf *args)
{
    iocshRun(args[0].sval, args[1].sval);
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
    {"exit",0,0};
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
