#ifdef RTEMS_LEGACY_STACK
#include "../RTEMS-kernel/devLibVMEOSD.c"
#else
#pragma message "\n VME-Support only with RTEMS Legacy stack\n" 
#include "../default/devLibVMEOSD.c"
#endif
