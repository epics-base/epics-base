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
#include <vector>
#include <map>
#include <string>

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#define EPICS_PRIVATE_API

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

#include "epicsReadlinePvt.h"

#if EPICS_COMMANDLINE_LIBRARY == EPICS_COMMANDLINE_LIBRARY_READLINE
#  include <readline/readline.h>
#  define USE_READLINE
/* libedit also provides readline.h, but isn't fully compatible with
 * GNU readline.  It also doesn't specifically identify itself.
 *
 * libedit pretends to be GNU readline 4.2 (circa 2001), but lacks
 * some definitions found in that GNU release, but has others not
 * added until later GNU releases.
 */
#  if RL_READLINE_VERSION == 0x402 && !defined(RL_STATE_NONE)
/* actual GNU readline 4.2 defines RL_STATE_NONE, but libedit.h
 * circa libedit-39 (aka. CVS 1.33) does not.  It also does not
 * provide rl_basic_quote_characters
 */
static const char *rl_basic_quote_characters;
#  endif
#endif

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
static epicsThreadPrivateId iocshContextId;

static void iocshInit (void);

/*
 * I/O redirection
 */
namespace {
struct iocshRedirect {
    // pointer within the current line buffer
    const char *name;
    // a static string constant: "a", "r", or "w"
    const char *mode;
    FILE       *fp;
    FILE       *oldFp;
    bool        mustRestore;

    iocshRedirect()
        :name(NULL)
        ,mode(NULL)
        ,fp(NULL)
        ,oldFp(NULL)
        ,mustRestore(false)
    {}

    ~iocshRedirect() {
        close();
    }

    void close() {
        if(fp) {
            (void)fclose(fp);
            fp = NULL;
        }
    }
};
} // namespace

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
static
void iocshRegisterImpl (const iocshFuncDef *piocshFuncDef,
    iocshCallFunc func)
{
    struct iocshCommand *l, *p, *n;
    int i;

    for (l = NULL, p = iocshCommandHead ; p != NULL ; l = p, p = p->next) {
        i = strcmp (piocshFuncDef->name, p->def.pFuncDef->name);
        if (i == 0) {
            p->def.pFuncDef = piocshFuncDef;
            p->def.func = func;
            return;
        }
        if (i < 0)
            break;
    }
    n = (struct iocshCommand *) callocMustSucceed (1, sizeof *n,
        "iocshRegister");
    if (!registryAdd(iocshCmdID, piocshFuncDef->name, (void *)n)) {
        free (n);
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
}

void epicsStdCall iocshRegister (const iocshFuncDef *piocshFuncDef,
    iocshCallFunc func)
{
    iocshTableLock ();
    iocshRegisterImpl (piocshFuncDef, func);
    iocshTableUnlock ();
}
/*
 * Retrieves a previously registered function with the given name.
 */
const iocshCmdDef * epicsStdCall iocshFindCommand(const char *name)
{
    return (iocshCmdDef *) registryFind(iocshCmdID, name);
}

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

char** (*iocshCompleteRecord)(const char *word) = NULL;

namespace {

struct Tokenize {
    // holds pointers to the most recently split() line buffer
    std::vector<const char*> argv;
    typedef  std::map<int, iocshRedirect> redirects_t;
    redirects_t redirects;
    iocshRedirect* redirect;
    bool noise;

    Tokenize()
        :redirect(NULL)
        ,noise(true)
    {}

    ~Tokenize()
    {
        stopRedirect();
    }

    // if split()==true then argv.size()>=1 where
    // argv[size()-1] is always a NULL.
    size_t size() const {
        return argv.empty() ? 0u : argv.size()-1u;
    }
    bool empty() const {
        return argv.size()<=1u;
    }

    bool is_redirected(int fd) const {
        redirects_t::const_iterator it = redirects.find(fd);
        return it!=redirects.end() && !!it->second.name;
    }

    /* Break line into words.
     * Stores pointers to line buffer this->argv and this->redirects[].name
     */
    bool split(const char *filename, int lineno, char *line, int& icin) {

        argv.clear();
        redirects.clear();
        redirect = NULL;

        int icout = 0;
        bool inword = false;
        bool backslash = false;
        char quote = 0;

        for (;;) {
            char c = line[icin++];
            if (c == '\0')
                break;

            bool sep = !quote && !backslash && (strchr (" \t(),\r", c));

            if (!quote && !backslash) {
                int redirectFd = 1;
                if (c == '\\') {
                    backslash = true;
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
                    quote = 0;
                }
                else {
                    if (!quote && !backslash) {
                        if (sep) {
                            inword = false;
                            // this "closes" a sub-string which was previously
                            // stored in argv[] or redirects[].name
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
                        if (redirect->name) { // double redirect?
                            return false;
                        }
                        redirect->name = line + icout;
                        redirect = NULL;
                    }
                    else {
                        argv.push_back(line + icout);
                    }
                    if (!quote)
                        line[icout++] = c;
                    inword = true;
                }
            }
            backslash = false;
        }

        if (inword)
            line[icout++] = '\0';


        if (redirect != NULL) {
            if(noise)
                showError(filename, lineno, "Illegal redirection.");
            return true;
        }
        if (quote) {
            if(noise)
                showError(filename, lineno, "Unbalanced quote.");
            return true;
        }
        if (backslash) {
            if(noise)
                showError(filename, lineno, "Trailing backslash.");
            return true;
        }

        argv.push_back(NULL);

        return false;
    }

    int openRedirect(const char *filename, int lineno)
    {
        for(redirects_t::iterator it = redirects.begin(), end = redirects.end();
            it!=end; ++it)
        {
            iocshRedirect *redirect = &it->second;
            if(!redirect->name)
                continue;

            redirect->fp = fopen(redirect->name, redirect->mode);
            if (redirect->fp == NULL) {
                int err = errno;
                showError(filename, lineno, "Can't open \"%s\": %s.",
                                            redirect->name, strerror(err));
                redirect->name = NULL;
                // caller will clear tok.redirects
                return -1;
            }
            redirect->mustRestore = false;
        }
        return 0;
    }

    void startRedirect()
    {
        for(redirects_t::iterator it = redirects.begin(), end = redirects.end();
            it!=end; ++it)
        {
            iocshRedirect *redirect = &it->second;
            if(!redirect->fp)
                continue;

            switch(it->first) {
            case 0:
                redirect->oldFp = epicsGetThreadStdin();
                epicsSetThreadStdin(redirect->fp);
                redirect->mustRestore = true;
                break;
            case 1:
                redirect->oldFp = epicsGetThreadStdout();
                epicsSetThreadStdout(redirect->fp);
                redirect->mustRestore = true;
                break;
            case 2:
                redirect->oldFp = epicsGetThreadStderr();
                epicsSetThreadStderr(redirect->fp);
                redirect->mustRestore = true;
                break;
            }
        }
    }

    void stopRedirect()
    {
        for(redirects_t::iterator it = redirects.begin(), end = redirects.end();
            it!=end; ++it)
        {
            iocshRedirect *redirect = &it->second;
            if(!redirect->fp)
                continue;

            if (redirect->mustRestore) {
                switch(it->first) {
                case 0: epicsSetThreadStdin(redirect->oldFp);  break;
                case 1: epicsSetThreadStdout(redirect->oldFp); break;
                case 2: epicsSetThreadStderr(redirect->oldFp); break;
                }
                redirect->mustRestore = false;
            }

            redirect->close();
            redirect->name = NULL;
        }
    }
};

#ifdef USE_READLINE

char* iocsh_complete_command(const char* word, int notfirst)
{
    // ick! ... readline is not re-entrant anyway
    static const iocshCommand *next;

    if(!notfirst) { // aka. first call
        next = iocshCommandHead;
    }

    const size_t wlen = strlen(word);

    while(next) {
        const iocshCommand *cur = next;
        next = next->next;

        if(strncmp(word, cur->def.pFuncDef->name, wlen)==0) {
            return strdup(cur->def.pFuncDef->name);
        }
    }

    return NULL;
}

char* iocsh_complete_variable(const char* word, int notfirst)
{
    // ick! ... readline is not re-entrant anyway
    static const iocshVariable *next;

    if(!notfirst) { // aka. first call
        next = iocshVariableHead;
    }

    const size_t wlen = strlen(word);

    while(next) {
        const iocshVariable *cur = next;
        next = next->next;

        if(strncmp(word, cur->pVarDef->name, wlen)==0) {
            return strdup(cur->pVarDef->name);
        }
    }

    return NULL;
}

char** iocsh_attempt_completion(const char* word, int start, int end)
{
    const char *line = rl_line_buffer;

    if(!line || !word || start<0 || end <0 || start>end)
        return NULL; // paranoia

    // skip any leading space
    while(isspace(*line)) {
        line++;
        start--;
        end--;
    }

    if(start==0) { // complete command name
        return rl_completion_matches(word, iocsh_complete_command);

    } else { // complete some argument
        // make a copy as split() will insert nils
        size_t linelen = strlen(line);
        std::vector<char> lbuf(linelen+1u);
        memcpy(&lbuf[0], line, linelen);
        lbuf[linelen] = '\0';

        int pos = 0;
        while(isspace(lbuf[pos]))
            pos++;

        Tokenize tokenize;
        // don't complain about "Unbalanced quote when completing
        tokenize.noise = false;
        bool err = tokenize.split(NULL, -1, &lbuf[0], pos);

        if(!err)
            err = tokenize.empty() && tokenize.redirects.empty();

        // look up command name
        iocshCmdDef *def = NULL;
        if(!err)
            err = !(def = (iocshCmdDef *) registryFind(iocshCmdID, tokenize.argv[0]));

        // Find out which argument 'start' is.
        // arg is index into tokenize.argv[]
        // argv[0] is command name, argv[1] would be first argument.
        size_t arg;
        if(!err) {
            for(arg=1; arg<tokenize.size(); arg++) {
                // BUG: does not work with eg. 'dbpr("X")' as split()
                //      has rewritten lbuf to make this 'dbpr "X"'
                size_t soffset = tokenize.argv[arg]-&lbuf[0];
                size_t eoffset = soffset + strlen(tokenize.argv[arg]);
                if(soffset >= size_t(start) && eoffset <= size_t(end)) {
                    break;
                }
            }
            if(arg-1u >= size_t(def->pFuncDef->nargs)) {
                if(def->pFuncDef->arg
                   && def->pFuncDef->nargs
                   && def->pFuncDef->arg[def->pFuncDef->nargs-1u]->type == iocshArgArgv)
                {
                    // last argument is variable length
                    arg = def->pFuncDef->nargs;
                } else {
                    err = true;
                }
            }
        }

        if(!err) {
            // we are being asked to complete a valid command,
            // for which we have split argument strings.

            const iocshArg * argdef = def->pFuncDef->arg[arg-1u];

            // known argument type
            rl_attempted_completion_over = 1;

            if(argdef->type==iocshArgStringRecord && iocshCompleteRecord) {
                return (*iocshCompleteRecord)(word);

            } else if(argdef->type==iocshArgStringPath) {
                // use default completion (filesystem)
                rl_attempted_completion_over = 0;

            } else if(argdef->type==iocshArgPdbbase) {
                char **ret = (char**)calloc(2, sizeof(*ret));
                if(ret)
                    ret[0] = strdup("pdbbase");
                return ret;

            } else if(arg==1 && strcmp(def->pFuncDef->name, "help")==0) {
                return rl_completion_matches(word, iocsh_complete_command);

            } else if(arg==1 && strcmp(def->pFuncDef->name, "var")==0) {
                return rl_completion_matches(word, iocsh_complete_variable);
            }

        }

        return NULL;
    }
}
#endif

struct ReadlineContext {
    void *context;
#ifdef USE_READLINE
    // readline on BSD/OSX missing 'rl_completion_func_t' typedef, and 'const'
    char* prev_rl_readline_name;
    char* prev_rl_basic_word_break_characters;
    char* prev_rl_completer_word_break_characters;
    char* prev_rl_basic_quote_characters;
    char* prev_rl_completer_quote_characters;
    //rl_completion_func_t* prev_rl_attempted_completion_function;
    char** (*prev_rl_attempted_completion_function)(const char* word, int start, int end);
#endif

    ReadlineContext()
        :context(NULL)
    {}

    bool setup(FILE *fp) {
        context = epicsReadlineBegin(fp);
#ifdef USE_READLINE
        if(context) {
            prev_rl_readline_name = (char*)rl_readline_name;
            prev_rl_basic_word_break_characters = (char*)rl_basic_word_break_characters;
            prev_rl_completer_word_break_characters = (char*)rl_completer_word_break_characters;
            prev_rl_basic_quote_characters = (char*)rl_basic_quote_characters;
            prev_rl_completer_quote_characters = (char*)rl_completer_quote_characters;
            prev_rl_attempted_completion_function = rl_attempted_completion_function;

            rl_readline_name = (char*)"iocsh";
            rl_basic_word_break_characters = (char*)"\t (),";
            rl_completer_word_break_characters = (char*)"\t (),";
            rl_basic_quote_characters = (char*)"\"";
            rl_completer_quote_characters = (char*)"\"";
            rl_attempted_completion_function = &iocsh_attempt_completion;
            rl_bind_key('\t', rl_complete);
        }
#endif
        return context;
    }

    ~ReadlineContext() {
        if(context) {
#ifdef USE_READLINE
            rl_readline_name = prev_rl_readline_name;
            rl_basic_word_break_characters = prev_rl_basic_word_break_characters;
            rl_completer_word_break_characters = prev_rl_completer_word_break_characters;
            rl_basic_quote_characters = prev_rl_basic_quote_characters;
            rl_completer_quote_characters = prev_rl_completer_quote_characters;
            rl_attempted_completion_function = prev_rl_attempted_completion_function;
            // cf. osdReadlineBegin() in gnuReadline.c
            rl_bind_key('\t', rl_insert);
#endif
            epicsReadlineEnd(context);
        }
    }
};

} // namespace

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
    case iocshArgStringRecord:
    case iocshArgStringPath:
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
    int icin;
    char c;
    const char *raw = NULL;;
    char *line = NULL;
    int lineno = 0;
    const char *prompt = NULL;
    std::vector<iocshArgBuf> argBuf;
    struct iocshCommand *found;
    ReadlineContext readline;
    int wasOkToBlock;
    static const char * pairs[] = {"", "environ", NULL, NULL};
    iocshScope scope;
    iocshContext *context;
    char ** defines = NULL;
    int ret = 0;
    Tokenize tokenize;

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
        if (!readline.setup(fp)) {
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
     * Parse macro definitions, this check occurs before creating the
     * macro handle to simplify cleanup.
     */

    if (macros) {
        if (macParseDefns(NULL, macros, &defines) < 0) {
            return -1;
        }
    }

    // Check for existing context or construct a new one.
    context = (iocshContext *) epicsThreadPrivateGet(iocshContextId);

    if (!context) {
        context = (iocshContext*)calloc(1, sizeof(*context));
        if (!context || macCreateHandle(&context->handle, pairs)) {
            errlogMessage("iocsh: macCreateHandle failed.");
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
            if ((raw = epicsReadline(prompt, readline.context)) == NULL)
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
        if (tokenize.split(filename, lineno, line, icin)) {
            scope.errored = true;
            continue;

        } else if (tokenize.empty() && tokenize.redirects.empty()) {
            break;
        }

        /*
         * Special case -- Redirected input but no command
         * Treat as if 'iocsh filename'.
         */
        if (tokenize.empty() && tokenize.is_redirected(0)) {
            const char* commandFile = tokenize.redirects[0].name;
            tokenize.redirects[0].name = NULL;
            if (tokenize.openRedirect(filename, lineno) < 0)
                continue;
            tokenize.startRedirect();
            if(iocshBody(commandFile, NULL, macros))
                scope.errored = true;
            tokenize.stopRedirect();
            continue;
        }

        /*
         * Special command?
         */
        if (!tokenize.empty() && (strcmp(tokenize.argv[0], "exit") == 0))
            break;

        /*
         * Set up redirection
         */
        if ((tokenize.openRedirect(filename, lineno) == 0) && !tokenize.empty()) {
            // error unless a function is actually called.
            // handles command not-found and arg parsing errors.
            scope.errored = true;
            /*
             * Look up command
             */
            found = (iocshCommand *)registryFind (iocshCmdID, tokenize.argv[0]);
            if (found) {
                /*
                 * Process arguments and call function
                 */
                struct iocshFuncDef const *piocshFuncDef = found->def.pFuncDef;
                // argBuf resize() with one extra so that &argBuf[0] doesn't trigger
                // windows debug assert.
                argBuf.resize(size_t(piocshFuncDef->nargs)+1u);
                for (size_t iarg = 0 ; ; ) {
                    if (iarg == size_t(piocshFuncDef->nargs)) {
                        tokenize.startRedirect();
                        /* execute */
                        scope.errored = false;
                        try {
                            (*found->def.func)(&argBuf[0]);
                        } catch(std::exception& e){
                            fprintf(epicsGetStderr(), "c++ error: %s\n", e.what());
                            scope.errored = true;
                        } catch(...) {
                            fprintf(epicsGetStderr(), "c++ error unknown\n");
                            scope.errored = true;
                        }
                        break;
                    }
                    if (piocshFuncDef->arg[iarg]->type == iocshArgArgv) {
                        argBuf[iarg].aval.ac = tokenize.size()-iarg;
                        argBuf[iarg].aval.av = const_cast<char**>(&tokenize.argv[iarg]);
                        iarg = piocshFuncDef->nargs;
                    }
                    else {
                        if (!cvtArg (filename, lineno,
                                ((iarg < tokenize.size()) ? const_cast<char*>(tokenize.argv[iarg+1]) : NULL),
                                &argBuf[iarg], piocshFuncDef->arg[iarg]))
                            break;
                        iarg++;
                    }
                }
                if ((prompt != NULL) && (strcmp(tokenize.argv[0], "epicsEnvSet") == 0)) {
                    const char *newPrompt;
                    if ((newPrompt = envGetConfigParamPtr(&IOCSH_PS1)) != NULL)
                        prompt = newPrompt;
                }
            }
            else {
                showError(filename, lineno, "Command %s not found.", tokenize.argv[0]);
            }
        }
        tokenize.stopRedirect();
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
    free(line);
    errlogFlush();
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
                                       "  wait - stall process for [value] seconds, then continue.\n"};
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

static void iocshOnce (void *)
{
    iocshTableMutex = epicsMutexMustCreate ();
    iocshContextId = epicsThreadPrivateCreate();
    epicsMutexMustLock (iocshTableMutex);
    iocshRegisterImpl(&commentFuncDef,commentCallFunc);
    iocshRegisterImpl(&exitFuncDef,exitCallFunc);
    iocshRegisterImpl(&helpFuncDef,helpCallFunc);
    iocshRegisterImpl(&iocshCmdFuncDef,iocshCmdCallFunc);
    iocshRegisterImpl(&iocshLoadFuncDef,iocshLoadCallFunc);
    iocshRegisterImpl(&iocshRunFuncDef,iocshRunCallFunc);
    iocshRegisterImpl(&onFuncDef, onCallFunc);
    iocshTableUnlock();
}

static void iocshInit (void)
{
    static epicsThreadOnceId iocshOnceId = EPICS_THREAD_ONCE_INIT;
    epicsThreadOnce (&iocshOnceId, iocshOnce, NULL);
}

} /* extern "C" */
