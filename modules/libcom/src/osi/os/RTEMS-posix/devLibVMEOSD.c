#ifdef RTEMS_LEGACY_STACK
#include "os/RTEMS-score/devLibVMEOSD.c"
#else
#pragma message "\n VME Support requires the RTEMS Legacy stack\n" 
#include "os/default/devLibVMEOSD.c"
#endif
