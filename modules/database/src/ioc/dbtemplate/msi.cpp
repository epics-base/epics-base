/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* EPICS Base is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution.
\*************************************************************************/

/* msi - macro substitutions and include */

#include <string>
#include <list>

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <dbDefs.h>
#include <macLib.h>
#include <errlog.h>
#include <epicsString.h>
#include <osiFileName.h>
#include <osiUnistd.h>

#define MAX_BUFFER_SIZE 4096
#define MAX_DEPS 1024

#if 0
/* Debug Tracing */
int din = 0;
#define ENTER fprintf(stderr, "%*sEntering %s\n", 2*din++, "", __FUNCTION__)

#define STEP(s) fprintf(stderr, "%*s%s: %s\n", 2*din, "", __FUNCTION__, s)
#define STEPS(s, v) fprintf(stderr, "%*s%s: %s '%s'\n", 2*din, "", __FUNCTION__, s, v)
#define STEPD(s, v) fprintf(stderr, "%*s%s: %s %d\n", 2*din, "", __FUNCTION__, s, v)

#define EXIT fprintf(stderr, "%*s%s: Returning\n", 2*din--, "", __FUNCTION__)
#define EXITD(r) fprintf(stderr, "%*s%s: Returning %d\n", 2*din--, "", __FUNCTION__, r)
#define EXITS(r) fprintf(stderr, "%*s%s: Returning '%s'\n", 2*din--, "", __FUNCTION__, r)
#else
#define ENTER

#define STEP(s)
#define STEPS(s, v)
#define STEPD(s, v)

#define EXIT
#define EXITD(r)
#define EXITS(r)
#endif


/* Module to read the template files */
typedef struct inputData inputData;

static void inputConstruct(inputData **ppvt);
static void inputDestruct(inputData * const pvt);
static void inputAddPath(inputData * const pvt, const char * const pval);
static void inputBegin(inputData * const pvt, const char * const fileName);
static char *inputNextLine(inputData * const pvt);
static void inputNewIncludeFile(inputData * const pvt, const char * const name);
static void inputErrPrint(const inputData * const pvt);

/* Module to read the substitution file */
typedef struct subInfo subInfo;

static void substituteOpen(subInfo **ppvt, const std::string& substitutionName);
static void substituteDestruct(subInfo * const pvt);
static bool substituteGetNextSet(subInfo * const pvt, char **filename);
static bool substituteGetGlobalSet(subInfo * const pvt);
static const char *substituteGetReplacements(subInfo * const pvt);
static const char *substituteGetGlobalReplacements(subInfo * const pvt);

/* Forward references to local routines */
static void usageExit(const int status);
static void abortExit(const int status);
static void addMacroReplacements(MAC_HANDLE * const macPvt,
                                 const char * const pval);
static void makeSubstitutions(inputData * const inputPvt,
                              MAC_HANDLE * const macPvt,
                              const char * const templateName);

/*Global variables */
static int opt_V = 0;
static bool opt_D = false;

static char *outFile = 0;
static int numDeps = 0, depHashes[MAX_DEPS];


int main(int argc,char **argv)
{
    inputData *inputPvt;
    MAC_HANDLE *macPvt;
    char *pval;
    std::string substitutionName;
    char *templateName = 0;
    bool localScope = true;

    inputConstruct(&inputPvt);
    macCreateHandle(&macPvt, 0);
    while ((argc > 1) && (argv[1][0] == '-')) {
        int narg = (strlen(argv[1]) == 2) ? 2 : 1;
        pval = (narg == 1) ? (argv[1] + 2) : argv[2];

        if (strncmp(argv[1], "-I", 2) == 0) {
            inputAddPath(inputPvt, pval);
        }
        else if (strcmp(argv[1], "-D") == 0) {
            opt_D = true;
            narg = 1; /* no argument for this option */
        }
        else if(strncmp(argv[1], "-o", 2) == 0) {
            outFile = epicsStrDup(pval);
        }
        else if(strncmp(argv[1], "-M", 2) == 0) {
            addMacroReplacements(macPvt, pval);
        }
        else if(strncmp(argv[1], "-S", 2) == 0) {
            substitutionName = pval;
        }
        else if (strcmp(argv[1], "-V") == 0) {
            opt_V = 1;
            narg = 1; /* no argument for this option */
        }
        else if (strcmp(argv[1], "-g") == 0) {
            localScope = false;
            narg = 1; /* no argument for this option */
        }
        else if (strcmp(argv[1], "-h") == 0) {
            usageExit(0);
        }
        else {
            fprintf(stderr, "msi: Bad argument \"%s\"\n", argv[1]);
            usageExit(1);
        }

        argc -= narg;
        for (int i = 1; i < argc; i++)
            argv[i] = argv[i + narg];
    }

    if (!opt_V)
        macSuppressWarning(macPvt, 1);

    if (argc > 2) {
        fprintf(stderr, "msi: Too many arguments\n");
        usageExit(1);
    }

    if (opt_D) {
        if (!outFile) {
            fprintf(stderr, "msi: Option -D requires -o for Makefile target\n");
            exit(1);
        }
        printf("%s:", outFile);
    }
    else if (outFile && freopen(outFile, "w", stdout) == NULL) {
        fprintf(stderr, "msi: Can't open %s for writing: %s\n",
            outFile, strerror(errno));
        exit(1);
    }

    if (argc == 2)
        templateName = epicsStrDup(argv[1]);

    if (substitutionName.empty()) {
        STEP("Single template+substitutions file");
        makeSubstitutions(inputPvt, macPvt, templateName);
    }
    else {
        subInfo *substitutePvt;
        char *filename = 0;
        bool isGlobal, isFile;

        STEPS("Substitutions from file", substitutionName.c_str());
        substituteOpen(&substitutePvt, substitutionName);
        do {
            isGlobal = substituteGetGlobalSet(substitutePvt);
            if (isGlobal) {
                STEP("Handling global macros");
                const char *macStr = substituteGetGlobalReplacements(substitutePvt);
                if (macStr)
                    addMacroReplacements(macPvt, macStr);
            }
            else if ((isFile = substituteGetNextSet(substitutePvt, &filename))) {
                if (templateName)
                    filename = templateName;
                if (!filename) {
                    fprintf(stderr, "msi: No template file\n");
                    usageExit(1);
                }

                STEPS("Handling template file", filename);
                const char *macStr;
                while ((macStr = substituteGetReplacements(substitutePvt))) {
                    if (localScope)
                        macPushScope(macPvt);

                    addMacroReplacements(macPvt, macStr);
                    makeSubstitutions(inputPvt, macPvt, filename);

                    if (localScope)
                        macPopScope(macPvt);
                }
            }
        } while (isGlobal || isFile);
        substituteDestruct(substitutePvt);
    }
    macDeleteHandle(macPvt);
    errlogFlush();  // macLib calls errlogPrintf()
    inputDestruct(inputPvt);
    if (opt_D) {
        printf("\n");
    }
    fflush(stdout);
    free(templateName);
    return opt_V & 2;
}

void usageExit(const int status)
{
    fprintf(stderr,
        "Usage: msi [options] [template]\n"
        "  stdin is used if neither template nor substitution file is given\n"
        "  options:\n"
        "    -h        Print this help message\n"
        "    -D        Output file dependencies, not substitutions\n"
        "    -V        Undefined macros generate an error\n"
        "    -g        All macros have global scope\n"
        "    -o<FILE>  Send output to <FILE>\n"
        "    -I<DIR>   Add <DIR> to include file search path\n"
        "    -M<SUBST> Add <SUBST> to (global) macro definitions\n"
        "              (<SUBST> takes the form VAR=VALUE,...)\n"
        "    -S<FILE>  Expand the substitutions in FILE\n");
    exit(status);
}

void abortExit(const int status)
{
    if (outFile) {
        fclose(stdout);
        unlink(outFile);
    }
    exit(status);
}

static void addMacroReplacements(MAC_HANDLE * const macPvt,
                                 const char * const pval)
{
    char **pairs;
    long status;

    status = macParseDefns(macPvt, pval, &pairs);
    if (status == -1) {
        fprintf(stderr, "msi: Error from macParseDefns\n");
        usageExit(1);
    }
    if (status) {
        status = macInstallMacros(macPvt, pairs);
        if (!status) {
            fprintf(stderr, "Error from macInstallMacros\n");
            usageExit(1);
        }
        free(pairs);
    }
}

typedef enum {cmdInclude,cmdSubstitute} cmdType;
static const char *cmdNames[] = {"include","substitute"};

static void makeSubstitutions(inputData * const inputPvt,
                              MAC_HANDLE * const macPvt,
                              const char * const templateName)
{
    char *input;
    static char buffer[MAX_BUFFER_SIZE];
    int  n;

    ENTER;
    inputBegin(inputPvt, templateName);
    while ((input = inputNextLine(inputPvt))) {
        int     expand=1;
        char    *p;
        char    *command = 0;

        p = input;
        /*skip whitespace at beginning of line*/
        while (*p && (isspace((int) *p))) ++p;

        /*Look for i or s */
        if (*p && (*p=='i' || *p=='s'))
            command = p;

        if (command) {
            char *pstart;
            char *pend;
            int  cmdind=-1;
            int  i;

            for (i = 0; i < NELEMENTS(cmdNames); i++) {
                if (strstr(command, cmdNames[i])) {
                    cmdind = i;
                }
            }
            if (cmdind < 0) goto endcmd;
            p = command + strlen(cmdNames[cmdind]);
            /*skip whitespace after command*/
            while (*p && (isspace((int) *p))) ++p;
            /*Next character must be quote*/
            if ((*p == 0) || (*p != '"')) goto endcmd;
            pstart = ++p;
            /*Look for end quote*/
            while (*p && (*p != '"')) {
                /*allow escape for embeded quote*/
                if ((p[0] == '\\') && p[1] == '"') {
                    p += 2;
                    continue;
                }
                else {
                    if (*p == '"') break;
                }
                ++p;
            }
            pend = p;
            if (*p == 0) goto endcmd;
            /*skip quote and any trailing blanks*/
            while (*++p == ' ') ;
            if (*p != '\n' && *p != 0) goto endcmd;
            std::string copy = std::string(pstart, pend);

            switch(cmdind) {
            case cmdInclude:
                inputNewIncludeFile(inputPvt, copy.c_str());
                break;

            case cmdSubstitute:
                addMacroReplacements(macPvt, copy.c_str());
                break;

            default:
                fprintf(stderr, "msi: Logic error in makeSubstitutions\n");
                inputErrPrint(inputPvt);
                abortExit(1);
            }
            expand = 0;
        }

endcmd:
        if (expand && !opt_D) {
            STEP("Expanding to output stream");
            n = macExpandString(macPvt, input, buffer, MAX_BUFFER_SIZE - 1);
            fputs(buffer, stdout);
            if (opt_V == 1 && n < 0) {
                fprintf(stderr, "msi: Error - undefined macros present\n");
                opt_V++;
            }
        }
    }
    EXIT;
}

typedef struct inputFile {
    std::string filename;
    FILE        *fp;
    int         lineNum;
} inputFile;

struct inputData {
    std::list<inputFile> inputFileList;
    std::list<std::string> pathList;
    char        inputBuffer[MAX_BUFFER_SIZE];
    inputData() { memset(inputBuffer, 0, sizeof(inputBuffer) * sizeof(inputBuffer[0])); };
};

static void inputOpenFile(inputData *pinputData, const char * const filename);
static void inputCloseFile(inputData *pinputData);
static void inputCloseAllFiles(inputData *pinputData);

static void inputConstruct(inputData **ppvt)
{
    *ppvt = new inputData;
}

static void inputDestruct(inputData * const pinputData)
{
    inputCloseAllFiles(pinputData);
    delete(pinputData);
}

static void inputAddPath(inputData * const pinputData, const char * const path)
{
    const char  *pcolon;
    const char  *pdir;
    size_t      len;
    const char  sep = *OSI_PATH_LIST_SEPARATOR;

    ENTER;
    pdir = path;
    /*an empty name at beginning, middle, or end means current directory*/
    while (pdir && *pdir) {
        bool emptyName = (*pdir == sep);
        if (emptyName) ++pdir;

        std::string directory;
        if (!emptyName) {
            pcolon = strchr(pdir, sep);
            len = (pcolon ? (pcolon - pdir) : strlen(pdir));
            if (len > 0)  {
                directory = std::string(pdir, len);
                pdir = pcolon;
                /*unless at end skip past first colon*/
                if (pdir && *(pdir + 1) != 0) ++pdir;
            }
            else { /*must have been trailing : */
                emptyName = true;
            }
        }

        if (emptyName) {
            directory = ".";
        }

        pinputData->pathList.push_back(directory);
    }
    EXIT;
}

static void inputBegin(inputData * const pinputData, const char * const fileName)
{
    ENTER;
    inputCloseAllFiles(pinputData);
    inputOpenFile(pinputData, fileName);
    EXIT;
}

static char *inputNextLine(inputData * const pinputData)
{
    std::list<inputFile>& inFileList = pinputData->inputFileList;

    ENTER;
    while (!inFileList.empty()) {
        inputFile& inFile = inFileList.front();
        char *pline = fgets(pinputData->inputBuffer, MAX_BUFFER_SIZE, inFile.fp);
        if (pline) {
            ++inFile.lineNum;
            EXITS(pline);
            return pline;
        }
        inputCloseFile(pinputData);
    }
    EXITD(0);
    return 0;
}

static void inputNewIncludeFile(inputData * const pinputData,
                                const char * const name)
{
    ENTER;
    inputOpenFile(pinputData,name);
    EXIT;
}

static void inputErrPrint(const inputData *const pinputData)
{
    ENTER;
    fprintf(stderr, "input: '%s' at ", pinputData->inputBuffer);
    const std::list<inputFile>& inFileList = pinputData->inputFileList;
    std::list<inputFile>::const_iterator inFileIt = inFileList.begin();
    while (inFileIt != inFileList.end()) {
        fprintf(stderr, "line %d of ", inFileIt->lineNum);

        if (!inFileIt->filename.empty()) {
            fprintf(stderr, " file %s\n", inFileIt->filename.c_str());
        }
        else {
            fprintf(stderr, "stdin:\n");
        }

        if (++inFileIt != inFileList.end()) {
            fprintf(stderr, "  included from ");
        }
        else {
            fprintf(stderr,"\n");
        }
    }
    fprintf(stderr,"\n");
    EXIT;
}

static void inputOpenFile(inputData *pinputData, const char * const filename)
{
    std::list<std::string>& pathList = pinputData->pathList;
    std::list<std::string>::iterator pathIt = pathList.end();
    std::string fullname;
    FILE        *fp = 0;

    ENTER;
    if (!filename) {
        STEP("Using stdin");
        fp = stdin;
    }
    else if (pathList.empty() || strchr(filename, '/')){
        STEPS("Opening ", filename);
        fp = fopen(filename, "r");
    }
    else {
        pathIt = pathList.begin();
        while(pathIt != pathList.end()) {
            fullname = *pathIt + "/" + filename;
            STEPS("Trying", filename);
            fp = fopen(fullname.c_str(), "r");
            if (fp)
                break;
            ++pathIt;
        }
    }

    if (!fp) {
        fprintf(stderr, "msi: Can't open file '%s'\n", filename);
        inputErrPrint(pinputData);
        abortExit(1);
    }

    STEP("File opened");
    inputFile inFile = inputFile();

    if (pathIt != pathList.end()) {
        inFile.filename = fullname;
    }
    else if (filename) {
        inFile.filename = filename;
    }
    else {
        inFile.filename = "stdin";
    }

    if (opt_D) {
        int hash = epicsStrHash(inFile.filename.c_str(), 12345);
        int i = 0;
        int match = 0;

        while (i < numDeps) {
            if (hash == depHashes[i++]) {
                match = 1;
                break;
            }
        }
        if (!match) {
            const char *wrap = numDeps ? " \\\n" : "";

            printf("%s %s", wrap, inFile.filename.c_str());
            if (numDeps < MAX_DEPS) {
                depHashes[numDeps++] = hash;
            }
            else {
                fprintf(stderr, "msi: More than %d dependencies!\n", MAX_DEPS);
                depHashes[0] = hash;
            }
        }
    }

    inFile.fp = fp;
    pinputData->inputFileList.push_front(inFile);
    EXIT;
}

static void inputCloseFile(inputData *pinputData)
{
    std::list<inputFile>& inFileList = pinputData->inputFileList;
    ENTER;
    if(!inFileList.empty()) {
        inputFile& inFile = inFileList.front();
        if (fclose(inFile.fp))
            fprintf(stderr, "msi: Can't close input file '%s'\n", inFile.filename.c_str());
        inFileList.erase(inFileList.begin());
    }
    EXIT;
}

static void inputCloseAllFiles(inputData *pinputData)
{
    ENTER;
    const std::list<inputFile>& inFileList = pinputData->inputFileList;
    while(!inFileList.empty()) {
        inputCloseFile(pinputData);
    }
    EXIT;
}

/*start of code that handles substitution file*/
typedef enum {
    tokenLBrace, tokenRBrace, tokenSeparator, tokenString, tokenEOF
} tokenType;

typedef struct subFile {
    std::string substitutionName;
    FILE        *fp;
    int         lineNum;
    char        inputBuffer[MAX_BUFFER_SIZE];
    char        *pnextChar;
    tokenType   token;
    char        string[MAX_BUFFER_SIZE];
} subFile;

struct subInfo {
    subFile     *psubFile;
    bool        isFile;
    char        *filename;
    bool        isPattern;
    std::list<std::string> patternList;
    std::string macroReplacements;
    subInfo() : psubFile(NULL), isFile(false), filename(NULL),
                isPattern(false) {};
};

static char *subGetNextLine(subFile *psubFile);
static tokenType subGetNextToken(subFile *psubFile);
static void subFileErrPrint(subFile *psubFile, const char * message);
static void freeSubFile(subInfo *psubInfo);
static void freePattern(subInfo *psubInfo);
static void catMacroReplacements(subInfo *psubInfo,const char *value);

void freeSubFile(subInfo *psubInfo)
{
    subFile *psubFile = psubInfo->psubFile;

    ENTER;
    if (psubFile->fp) {
        if (fclose(psubFile->fp))
            fprintf(stderr, "msi: Can't close substitution file\n");
    }
    delete(psubFile);
    free(psubInfo->filename);
    psubInfo->psubFile = 0;
    EXIT;
}

void freePattern(subInfo *psubInfo)
{
    ENTER;
    psubInfo->patternList.clear();
    psubInfo->isPattern = false;
    EXIT;
}

static void substituteDestruct(subInfo * const psubInfo)
{
    ENTER;
    freeSubFile(psubInfo);
    freePattern(psubInfo);
    delete(psubInfo);
    EXIT;
}

static void substituteOpen(subInfo **ppvt, const std::string& substitutionName)
{
    subInfo     *psubInfo;
    subFile     *psubFile;
    FILE        *fp;

    ENTER;
    psubInfo = new subInfo;
    *ppvt = psubInfo;
    psubFile = new subFile;
    psubInfo->psubFile = psubFile;

    fp = fopen(substitutionName.c_str(), "r");
    if (!fp) {
        fprintf(stderr, "msi: Can't open file '%s'\n", substitutionName.c_str());
        abortExit(1);
    }

    psubFile->substitutionName = substitutionName;
    psubFile->fp = fp;
    psubFile->lineNum = 1;
    psubFile->inputBuffer[0] = 0;
    psubFile->pnextChar = &psubFile->inputBuffer[0];
    subGetNextToken(psubFile);
    EXIT;
}

static bool substituteGetGlobalSet(subInfo * const psubInfo)
{
    subFile *psubFile = psubInfo->psubFile;

    ENTER;
    while (psubFile->token == tokenSeparator)
        subGetNextToken(psubFile);

    if (psubFile->token == tokenString &&
        strcmp(psubFile->string, "global") == 0) {
        subGetNextToken(psubFile);
        EXITD(1);
        return true;
    }

    EXITD(0);
    return false;
}

static bool substituteGetNextSet(subInfo * const psubInfo,char **filename)
{
    subFile     *psubFile = psubInfo->psubFile;

    ENTER;
    *filename = 0;
    while (psubFile->token == tokenSeparator)
        subGetNextToken(psubFile);

    if (psubFile->token == tokenEOF) {
        EXITD(0);
        return false;
    }

    if (psubFile->token == tokenString &&
        strcmp(psubFile->string, "file") == 0) {
        size_t len;

        STEP("Parsed 'file'");
        psubInfo->isFile = true;
        if (subGetNextToken(psubFile) != tokenString) {
            subFileErrPrint(psubFile, "Parse error, expecting a filename");
            abortExit(1);
        }

        freePattern(psubInfo);
        free(psubInfo->filename);

        len = strlen(psubFile->string);
        if (psubFile->string[0] == '"' &&
            psubFile->string[len - 1] == '"') {
            psubFile->string[len - 1] = '\0';
            psubInfo->filename = macEnvExpand(psubFile->string + 1);
        }
        else
            psubInfo->filename = macEnvExpand(psubFile->string);
        STEPS("Parsed filename", psubInfo->filename);

        while (subGetNextToken(psubFile) == tokenSeparator);

        if (psubFile->token != tokenLBrace) {
            subFileErrPrint(psubFile, "Parse error, expecting '{'");
            abortExit(1);
        }
        STEP("Parsed '{'");
        subGetNextToken(psubFile);
    }
    *filename = psubInfo->filename;

    while (psubFile->token == tokenSeparator)
        subGetNextToken(psubFile);

    if (psubFile->token == tokenLBrace) {
        EXITD(1);
        return true;
    }

    if (psubFile->token == tokenRBrace) {
        subFileErrPrint(psubFile, "Parse error, unexpected '}'");
        abortExit(1);
    }

    if (psubFile->token != tokenString ||
        strcmp(psubFile->string, "pattern") != 0) {
        subFileErrPrint(psubFile, "Parse error, expecting 'pattern'");
        abortExit(1);
    }

    STEP("Parsed 'pattern'");
    freePattern(psubInfo);
    psubInfo->isPattern = true;

    while (subGetNextToken(psubFile) == tokenSeparator);

    if (psubFile->token != tokenLBrace) {
        subFileErrPrint(psubFile, "Parse error, expecting '{'");
        abortExit(1);
    }
    STEP("Parsed '{'");

    while (true) {
        while (subGetNextToken(psubFile) == tokenSeparator);

        if (psubFile->token != tokenString)
            break;

        psubInfo->patternList.push_back(psubFile->string);
    }

    if (psubFile->token != tokenRBrace) {
        subFileErrPrint(psubFile, "Parse error, expecting '}'");
        abortExit(1);
    }

    subGetNextToken(psubFile);
    EXITD(1);
    return true;
}

static const char *substituteGetGlobalReplacements(subInfo * const psubInfo)
{
    subFile     *psubFile = psubInfo->psubFile;

    ENTER;
    psubInfo->macroReplacements.clear();

    while (psubFile->token == tokenSeparator)
        subGetNextToken(psubFile);

    if (psubFile->token == tokenRBrace && psubInfo->isFile) {
        psubInfo->isFile = false;
        free(psubInfo->filename);
        psubInfo->filename = 0;
        freePattern(psubInfo);
        subGetNextToken(psubFile);
        EXITD(0);
        return 0;
    }

    if (psubFile->token == tokenEOF) {
        EXITD(0);
        return 0;
    }
    if (psubFile->token != tokenLBrace) {
        EXITD(0);
        return 0;
    }

    while (true) {
        switch(subGetNextToken(psubFile)) {
            case tokenRBrace:
                subGetNextToken(psubFile);
                EXITS(psubInfo->macroReplacements.c_str());
                return psubInfo->macroReplacements.c_str();

            case tokenSeparator:
                catMacroReplacements(psubInfo, ",");
                break;

            case tokenString:
                catMacroReplacements(psubInfo, psubFile->string);
                break;

            case tokenLBrace:
                subFileErrPrint(psubFile, "Parse error, unexpected '{'");
                abortExit(1);
            case tokenEOF:
                subFileErrPrint(psubFile, "Parse error, incomplete file?");
                abortExit(1);
        }
    }
}

static const char *substituteGetReplacements(subInfo * const psubInfo)
{
    subFile     *psubFile = psubInfo->psubFile;

    ENTER;
    psubInfo->macroReplacements.clear();

    while (psubFile->token == tokenSeparator)
        subGetNextToken(psubFile);

    if (psubFile->token==tokenRBrace && psubInfo->isFile) {
        psubInfo->isFile = false;
        free(psubInfo->filename);
        psubInfo->filename = 0;
        freePattern(psubInfo);
        subGetNextToken(psubFile);
        EXITD(0);
        return 0;
    }

    if (psubFile->token == tokenEOF) {
        EXITD(0);
        return 0;
    }

    if (psubFile->token != tokenLBrace) {
        EXITD(0);
        return 0;
    }

    if (psubInfo->isPattern) {
        bool gotFirstPattern = false;

        while (subGetNextToken(psubFile) == tokenSeparator);
        std::list<std::string>& patternList = psubInfo->patternList;
        std::list<std::string>::iterator patternIt = patternList.begin();
        while (true) {
            if (psubFile->token == tokenRBrace) {
                subGetNextToken(psubFile);
                EXITS(psubInfo->macroReplacements.c_str());
                return psubInfo->macroReplacements.c_str();
            }

            if (psubFile->token != tokenString) {
                subFileErrPrint(psubFile,"Parse error, expecting macro value");
                abortExit(1);
            }

            if (gotFirstPattern)
                catMacroReplacements(psubInfo, ",");
            gotFirstPattern = true;

            if (patternIt != patternList.end()) {
                catMacroReplacements(psubInfo, patternIt->c_str());
                catMacroReplacements(psubInfo, "=");
                catMacroReplacements(psubInfo, psubFile->string);
                ++patternIt;
            }
            else {
                subFileErrPrint(psubFile, "Warning, too many values given");
            }

            while (subGetNextToken(psubFile) == tokenSeparator);
        }
    }
    else while(true) {
        switch(subGetNextToken(psubFile)) {
            case tokenRBrace:
                subGetNextToken(psubFile);
                EXITS(psubInfo->macroReplacements.c_str());
                return psubInfo->macroReplacements.c_str();

            case tokenSeparator:
                catMacroReplacements(psubInfo, ",");
                break;

            case tokenString:
                catMacroReplacements(psubInfo, psubFile->string);
                break;

            case tokenLBrace:
                subFileErrPrint(psubFile, "Parse error, unexpected '{'");
                abortExit(1);
            case tokenEOF:
                subFileErrPrint(psubFile, "Parse error, incomplete file?");
                abortExit(1);
        }
    }
}

static char *subGetNextLine(subFile *psubFile)
{
    char *pline;

    ENTER;
    do {
        pline = fgets(psubFile->inputBuffer, MAX_BUFFER_SIZE, psubFile->fp);
        ++psubFile->lineNum;
    } while (pline && psubFile->inputBuffer[0] == '#');

    if (!pline) {
        psubFile->token = tokenEOF;
        psubFile->inputBuffer[0] = 0;
        psubFile->pnextChar = 0;
        EXITD(0);
        return 0;
    }

    psubFile->pnextChar = &psubFile->inputBuffer[0];
    EXITS(&psubFile->inputBuffer[0]);
    return &psubFile->inputBuffer[0];
}

static void subFileErrPrint(subFile *psubFile, const char * message)
{
    fprintf(stderr, "msi: %s\n",message);
    fprintf(stderr, "  in substitution file '%s' at line %d:\n  %s",
            psubFile->substitutionName.c_str(), psubFile->lineNum,
            psubFile->inputBuffer);
}


static tokenType subGetNextToken(subFile *psubFile)
{
    char *p, *pto;

    ENTER;
    p = psubFile->pnextChar;
    if (!p) {
        STEP("Got EOF");
        psubFile->token = tokenEOF;
        goto done;
    }

    if (*p == 0 || *p == '\n' || *p == '#') {
        STEP("Got newline/comment");
        p = subGetNextLine(psubFile);
        psubFile->token = p ? tokenSeparator : tokenEOF;
        goto done;
    }

    while (isspace((int) *p)) p++;
    if (*p == '{') {
        STEP("Got '{'");
        psubFile->token = tokenLBrace;
        psubFile->pnextChar = ++p;
        goto done;
    }
    if (*p == '}') {
        STEP("Got '}'");
        psubFile->token = tokenRBrace;
        psubFile->pnextChar = ++p;
        goto done;
    }
    if (*p == 0 || isspace((int) *p) || *p == ',') {
        STEP("Got space/comma");
        while (isspace((int) *p) || *p == ',') p++;
        psubFile->token = tokenSeparator;
        psubFile->pnextChar = p;
        goto done;
    }
    /*now handle quoted strings*/
    if (*p == '"') {
        STEP("Got '\"'");
        pto = &psubFile->string[0];
        *pto++ = *p++;
        while (*p != '"') {
            if (*p == 0 || *p == '\n') {
                subFileErrPrint(psubFile, "Strings must be on single line\n");
                abortExit(1);
            }
            /*allow  escape for embeded quote*/
            if ((p[0] == '\\') && p[1] == '"') {
                *pto++ = *p++;
                *pto++ = *p++;
                continue;
            }
            *pto++ = *p++;
        }
        *pto++ = *p++;
        psubFile->pnextChar = p;
        *pto = 0;
        psubFile->token = tokenString;
        goto done;
    }

    /*Now take anything up to next non String token and not space*/
    pto = &psubFile->string[0];

    while (!isspace((int) *p) && (strspn(p, "\",{}") == 0))
        *pto++ = *p++;
    *pto = 0;
    STEPS("Got bareword", psubFile->string);

    psubFile->pnextChar = p;
    psubFile->token = tokenString;

done:
    EXITD(psubFile->token);
    return psubFile->token;
}

static void catMacroReplacements(subInfo *psubInfo, const char *value)
{
    ENTER;
    STEPS("Appending", value);
    psubInfo->macroReplacements += value;
    EXIT;
}
