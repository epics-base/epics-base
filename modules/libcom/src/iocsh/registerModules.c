#include "registerModules.h"
#include "epicsThread.h"
#include <cantProceed.h>
#include <dbDefs.h>
#include <ellLib.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <freeList.h>
#include <shareLib.h>
#include <stdio.h>
#include <string.h>
#include <valgrind/valgrind.h>

typedef struct {
    ELLNODE node;
    char *name;
    char *version;
} moduleInfo;

typedef struct {
    ELLLIST list;
    epicsMutexId mutex;
} registerList;

static registerList moduleRegistry;
static epicsThreadOnceId onceInitModuleRegistry = EPICS_THREAD_ONCE_INIT;

static moduleInfo *allocateModule(const char *name, const char *version) {
    moduleInfo *module = callocMustSucceed(1, sizeof(*module),
                                           "Fail to allocate module structure");
    module->name = epicsStrDup(name);
    module->version = epicsStrDup(version);

    return module;
}

static void initLinkedList(void *unused) {
    ellInit(&(moduleRegistry.list));
    moduleRegistry.mutex = epicsMutexMustCreate();
}

void registerPrintModules() {
    ELLNODE *cur;
    epicsThreadOnce(&onceInitModuleRegistry, initLinkedList, NULL);
    epicsMutexMustLock(moduleRegistry.mutex);
    printf("Module list:\n");
    for(cur = ellFirst(&(moduleRegistry.list)); cur; cur = ellNext(cur)) {
        moduleInfo *module = CONTAINER(cur, moduleInfo, node);
        printf("    %s: %s\n", module->name, module->version);
    }
    epicsMutexUnlock(moduleRegistry.mutex);
}

int registerModule(const char *name, const char *version) {
    epicsThreadOnce(&onceInitModuleRegistry, initLinkedList, NULL);
    moduleInfo *module = allocateModule(name, version);
    epicsMutexMustLock(moduleRegistry.mutex);
    ellAdd(&(moduleRegistry.list), &(module->node));
    epicsMutexUnlock(moduleRegistry.mutex);
    return 0;
}
