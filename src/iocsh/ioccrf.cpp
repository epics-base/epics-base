/* ioccrf.c */
/* Author:  Marty Kraimer Date: 27APR2000 */
/* Heavily modified by Eric Norum   Date: 03MAY2000 */
/* Adapted to C++ by Eric Norum   Date: 18DEC2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
#include <string>
#include <map>
using namespace std;

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "errlog.h"
#include "dbAccess.h"
#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "epicsReadline.h"

class IoccrfFunc {
  public:
    ioccrfFuncDef const *pFuncDef;
    ioccrfCallFunc func;
    IoccrfFunc(): pFuncDef(NULL), func(NULL) { }
    IoccrfFunc(const ioccrfFuncDef *d, ioccrfCallFunc f): pFuncDef(d), func(f){}
    IoccrfFunc& operator=(const IoccrfFunc& rhs) { pFuncDef=rhs.pFuncDef;
                                                   func=rhs.func;
                                                   return *this; }
    ~IoccrfFunc() { }
};

/*
 * We don't want the overhead of C++ strings, so use C strings
 */
struct Cstring_cmp {
    bool operator()(const char *p, const char *q) const {return strcmp(p,q)<0;}
};

/*
 * The table of commands
 */
namespace {  map<const char *,IoccrfFunc,Cstring_cmp> commands; }

/*
 * Register a command
 */
void epicsShareAPI ioccrfRegister (const ioccrfFuncDef *pioccrfFuncDef, ioccrfCallFunc func)
{
    commands[pioccrfFuncDef->name] = IoccrfFunc(pioccrfFuncDef, func);
}

/*
 * Free storage created by ioccrfRegister
 */
void epicsShareAPI ioccrfFree(void) 
{
    commands.clear();
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

    if (fp == NULL)
#ifdef IOCSH_USE_READLINE
        return epics_readline (prompt);
#else
        fp = stdin;
#endif
    line = (char *)malloc (linesize * sizeof *line);
    if (line == NULL) {
        printf ("Out of memory!\n");
        return NULL;
    }
    if (prompt)
        fputs (prompt, stdout);
    while ((c = getc (fp)) !=  '\n') {
        if (c == EOF) {
            free (line);
            return NULL;
        }
        if ((linelen + 1) >= linesize) {
            char *cp;

            linesize += 50;
            cp = (char *)realloc (line, linesize * sizeof *line);
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
    fputc ('\n', stderr);
    va_end (ap);
}

static int
cvtArg (const char *filename, int lineno, char *arg, ioccrfArgBuf *argBuf, const ioccrfArg *pioccrfArg)
{
    char *endp;

    switch (pioccrfArg->type) {
    case ioccrfArgInt:
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

    case ioccrfArgDouble:
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

    case ioccrfArgString:
        argBuf->sval = arg;
        break;

    case ioccrfArgPdbbase:
        /* Argument must be missing or 0 or pdbbase */
        if(!arg || !*arg || (*arg == '0') || (strcmp(arg, "pdbbase") == 0)) {
            argBuf->vval = pdbbase;
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
    FILE *fp = NULL;
    const char *filename = NULL;
    int icin, icout;
    int c, quote, inword, backslash;
    char *line = NULL;
    int lineno = 0;
    int argc;
    char **argv = NULL;
    int argvCapacity = 0;
    int sep;
    const char *prompt;
    const char *ifs = " \t(),";
    ioccrfArgBuf *argBuf = NULL;
    int argBufCapacity = 0;
    ioccrfFuncDef const *pioccrfFuncDef;
    map<const char *,IoccrfFunc>::iterator cmdIter;
    
    /*
     * See if command interpreter is interactive
     */
    if ((pathname == NULL) || (strcmp (pathname, "<telnet>") == 0)) {
        const char *historySize;
        if ((prompt = getenv ("IOCSH_PS1")) == NULL)
            prompt = "iocsh> ";
        if (((historySize = getenv ("IOCSH_HISTSIZE")) == NULL)
         && ((historySize = getenv ("HISTSIZE")) == NULL))
            historySize = "10";
        if (pathname == NULL) {
            epics_stifle_history (atoi (historySize));
            /*
             * FIXME: Could enable tab-completion of commands here
             */
            epics_bind_keys();
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
        line = my_readline (fp, prompt);
        if (line == NULL)
            break;

        /*
         * If using readline, add non-blank lines to history
         */
        if ((fp == NULL) && *line)
            epics_add_history (line);

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
                    for (cmdIter = commands.begin() ; cmdIter != commands.end() ; cmdIter++) {
                        pioccrfFuncDef = cmdIter->second.pFuncDef;
                        l = strlen (pioccrfFuncDef->name);
                        if ((l + col) >= 79) {
                            putchar ('\n');
                            col = 0;
                        }
                        fputs (pioccrfFuncDef->name, stdout);
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
                }
                else {
                    for (int i = 1 ; i < argc ; i++) {
                        cmdIter = commands.find(argv[i]);
                        if (cmdIter == commands.end()) {
                            printf ("%s -- no such command.\n", argv[i]);
                        }
                        else {
                            pioccrfFuncDef = cmdIter->second.pFuncDef;
                            fputs (pioccrfFuncDef->name, stdout);
                            for (int a = 0 ; a < pioccrfFuncDef->nargs ; a++) {
                                const char *cp = pioccrfFuncDef->arg[a]->name;
                                putchar (' ');
                                while (*cp != '\0') {
                                    if (isspace (*cp))
                                        putchar ('_');
                                    else
                                        putchar (*cp);
                                    cp++;
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
            cmdIter = commands.find(argv[0]);
            if (cmdIter == commands.end()) {
                showError (filename, lineno, "Command %s not found.", argv[0]);
                continue;
            }
            pioccrfFuncDef = cmdIter->second.pFuncDef;

            /*
             * Process arguments
             * Must make local copy of pioccrfFuncDef->arg[arg] to
             * ensure that this routine is thread-safe.
             */
            for (int arg = 0 ; ; arg++) {
                char *p = (arg < argc) ? argv[arg+1] : NULL;

                if (arg == pioccrfFuncDef->nargs) {
                    (*cmdIter->second.func)(argBuf);
                    break;
                }
                if (arg >= argBufCapacity) {
                    void *np;

                    argBufCapacity += 20;
                    np = realloc (argBuf, argBufCapacity * sizeof *argBuf);
                    if (np == NULL) {
                        fprintf (stderr, "Out of memory!\n");
                        argBufCapacity -= 20;
                        continue;
                    }
                    argBuf = (ioccrfArgBuf *)np;
                }
                if (!cvtArg (filename, lineno, p, &argBuf[arg], pioccrfFuncDef->arg[arg]))
                    break;
            }
        }
    }
    if (fp && (fp != stdin))
        fclose (fp);
    free (line);
    free (argv);
    free (argBuf);
    return 0;
}

/*
 * Dummy internal commands -- register and install in command table
 * so they show up in the help display
 */

extern "C" {
/* help */
static const ioccrfArg helpArg0 = { "command",ioccrfArgInt};
static const ioccrfArg *helpArgs[1] = {&helpArg0};
static const ioccrfFuncDef helpFuncDef =
    {"help",1,helpArgs};
static void helpCallFunc(const ioccrfArgBuf *args)
{
}

/* exit */
static const ioccrfFuncDef exitFuncDef =
    {"exit",0,0};
static void exitCallFunc(const ioccrfArgBuf *args)
{
}

static void localRegister (void)
{
    ioccrfRegister(&helpFuncDef,helpCallFunc);
    ioccrfRegister(&exitFuncDef,exitCallFunc);
}

} /* extern "C" */

/*
 * Register commands on application startup
 */
#include "ioccrfRegisterCommon.h"
class IoccrfRegister {
  public:
    IoccrfRegister() { localRegister(); ioccrfRegisterCommon(); }
};
static IoccrfRegister ioccrfRegisterObj;
