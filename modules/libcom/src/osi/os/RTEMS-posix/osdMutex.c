#include <rtems.h>
#include <rtems/error.h>
#include <rtems/rtems/tasks.h>
#ifndef RTEMS_LEGACY_STACK
#include <rtems/score/threadimpl.h>
#endif

#include "../posix/osdMutex.c"
