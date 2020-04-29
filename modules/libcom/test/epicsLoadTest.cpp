/*************************************************************************\
* Copyright (c) 2020 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <sstream>

#include <stdlib.h>

#include "epicsUnitTest.h"
#include "testMain.h"

#include "envDefs.h"
#include "epicsFindSymbol.h"
#include "epicsThread.h"

namespace {

void loadBad()
{
    testOk1(!epicsFindSymbol("noSuchFunction"));
}

// lookup a symbol from libCom
// which this executable is linked against (maybe statically)
// Doesn't work for static builds on windows
void loadCom()
{
    testDiag("Lookup symbol from Com");

#if defined (_WIN32) && defined(LINKING_STATIC)
    testTodoBegin("windows static build");
#endif

    void* ptr = epicsFindSymbol("epicsThreadGetCPUs");
    testOk(ptr==(void*)&epicsThreadGetCPUs,
           "%p == %p (epicsThreadGetCPUs) : %s",
           ptr, (void*)&epicsThreadGetCPUs,
           epicsLoadError());

    testTodoEnd();
}

void loadCA()
{
    testDiag("Load and lookup symbol from libca");

    std::string libname;
    {
        std::ostringstream strm;
        // running in eg. modules/libcom/test/O.linux-x86_64-debug
#ifdef _WIN32
        strm<<"..\\..\\..\\..\\bin\\"<<envGetConfigParamPtr(&EPICS_BUILD_TARGET_ARCH)<<"\\ca.dll";
#else
        strm<<"../../../../lib/"<<envGetConfigParamPtr(&EPICS_BUILD_TARGET_ARCH)<<"/";
#  ifdef __APPLE__
        strm<<"libca.dylib";
#  else
        strm<<"libca.so";
#  endif
#endif
        libname = strm.str();
    }
    testDiag("Loading %s", libname.c_str());

    void *ptr = epicsLoadLibrary(libname.c_str());
    testOk(!!ptr, "Loaded %p : %s", ptr, epicsLoadError());

    ptr = epicsFindSymbol("dbf_text");
    testOk(!!ptr, "dbf_text %p : %s", ptr, epicsLoadError());
}

} // namespace

MAIN(epicsLoadTest)
{
    testPlan(4);

    // reference to ensure linkage when linking statically,
    // and actually use the result to make extra doubly sure that
    // this call isn't optimized out by eg. an LTO pass.
    testDiag("# of CPUs %d", epicsThreadGetCPUs());

    loadBad();
#if defined(__rtems__) || defined(vxWorks)
    testSkip(3, "Target does not implement epicsFindSymbol()");
#else
    loadCom();
#  if defined(BUILDING_SHARED_LIBRARIES)
    loadCA();
#  else
    testSkip(2, "Shared libraries not built");
#  endif
#endif
    return testDone();
}
