/*************************************************************************\
* Copyright (C) 2017 Freddie Akeroyd
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <windows.h>
#include <dbghelp.h>
#define epicsExportSharedSymbols
#include "epicsStackTracePvt.h"
#include "epicsStackTrace.h"
#include "epicsString.h"
#include "epicsStdio.h"
#include "gpHash.h"

#define MAX_SYM_SIZE 255

/*
 * we cache a list of previously located symbols, currently just to
 * avoid a memory leak in allocating  sym_p->s_nam  and not
 * to save on a  SymFromAddr()  call. This was done in case libraries
 * were dynamically unloaded/reloaded and the cached answer might not
 * then be correct, but this may be over cautious.
 */
static struct gphPvt* symbol_table;

typedef struct
{
    char* name;
    void* value;
    char* file;
    char* fileAndLine;
    size_t line;
} SymbolData;

int epicsFindAddr(void *addr, epicsSymbol *sym_p)
{
    static int first_call = 1;
    static HANDLE process = NULL;
    DWORD64 displacement64 = 0;
    DWORD displacement = 0;
    SYMBOL_INFO *symbol;
    IMAGEHLP_LINE64 line;
    GPHENTRY* symbol_entry;
    SymbolData* symbol_data;
    size_t len;
    if (first_call)
    {
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
        process = GetCurrentProcess();
        SymInitialize(process, NULL, TRUE);
        gphInitPvt(&symbol_table, 256); /* table size must be a power of 2 in range 256 to 65536 */
        first_call = 0;
    }
    symbol = (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + (MAX_SYM_SIZE + 1) * sizeof(char), 1);
    symbol->MaxNameLen = MAX_SYM_SIZE;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    if (!SymFromAddr(process, (DWORD64)addr, &displacement64, symbol))
    {
        sym_p->s_nam = 0;
        sym_p->s_val = 0;
        sym_p->f_nam = 0;
        free(symbol);
        return -1;
    }
    if ( (symbol_entry = gphFind(symbol_table, symbol->Name, addr)) != NULL )
    {
        symbol_data = (SymbolData*)symbol_entry->userPvt;
    }
    else
    {
        symbol_entry = gphAdd(symbol_table, symbol->Name, addr);
        symbol_data = (SymbolData*)calloc(sizeof(SymbolData), 1);
        symbol_data->name = strdup(symbol->Name);
        symbol_data->value = (void*)symbol->Address;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        if (SymGetLineFromAddr64(process, (DWORD64)addr, &displacement, &line))
        {
            symbol_data->file = strdup(line.FileName);
            symbol_data->line = line.LineNumber;
            len = strlen(line.FileName) + 32; /* add enough for [line %lu] */
            symbol_data->fileAndLine = calloc((len + 1) * sizeof(char), 1);
            epicsSnprintf(symbol_data->fileAndLine, len, "%s[line %lu]", line.FileName, (unsigned long)line.LineNumber);
        }
        symbol_entry->userPvt = symbol_data;
    }
    sym_p->s_nam = symbol_data->name;
    sym_p->s_val = symbol_data->value;
    sym_p->f_nam = symbol_data->fileAndLine;
    free(symbol);
    return 0;
}

int epicsFindAddrGetFeatures(void)
{
#if defined(_MINGW)
/* 64bit MINGW can lookup DLL public symbols, 32bit cannot - but we need to be a dynamic build to have a DLL to lookup */
#   if defined(_WIN64) && defined(EPICS_CALL_DLL)
        return EPICS_STACKTRACE_DYN_SYMBOLS;
#   else
        return 0; /* only able to print addresses */
#   endif /* _WIN64 && EPICS_CALL_DLL */
#else
    return  EPICS_STACKTRACE_LCL_SYMBOLS
        | EPICS_STACKTRACE_GBL_SYMBOLS
        | EPICS_STACKTRACE_DYN_SYMBOLS;
#endif /* _MINGW */
}
