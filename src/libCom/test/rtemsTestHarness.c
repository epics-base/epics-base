/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * RTEMS initialization for EPICS tests
 *
 * This is part of the work being done to provide a unified set of automated
 * tests for EPICS.  Many more changes will be forthcoming.
 */
#include <bsp.h>

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_EXECUTIVE_RAM_SIZE        (1000*1024)
#define CONFIGURE_MAXIMUM_TASKS             rtems_resource_unlimited(30)
#define CONFIGURE_MAXIMUM_SEMAPHORES        rtems_resource_unlimited(200)
#define CONFIGURE_MAXIMUM_TIMERS            rtems_resource_unlimited(20)
#define CONFIGURE_MAXIMUM_MESSAGE_QUEUES    rtems_resource_unlimited(5)

#define CONFIGURE_TEST_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_TEST_NEEDS_CLOCK_DRIVER
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM

#define CONFIGURE_MICROSECONDS_PER_TICK          10000
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 30

#define CONFIGURE_INIT_TASK_STACK_SIZE        (10*1024)
#define CONFIGURE_INIT_TASK_PRIORITY          100
#define CONFIGURE_INIT_TASK_INITIAL_MODES (RTEMS_PREEMPT | \
                                           RTEMS_NO_TIMESLICE | \
                                           RTEMS_NO_ASR | \
                                           RTEMS_INTERRUPT_LEVEL(0))
#define CONFIGURE_INIT_TASK_ATTRIBUTES    (RTEMS_LOCAL | RTEMS_FLOATING_POINT)

#define CONFIGURE_INIT
rtems_task Init (rtems_task_argument argument);

#include <rtems/confdefs.h>

/*
 * Dummy network configuration
 */
#include <rtems/rtems_bsdnet.h>
extern void rtems_bsdnet_loopattach();
static struct rtems_bsdnet_ifconfig loopback_config = {
    "lo0",                          /* name */
    (int (*)(struct rtems_bsdnet_ifconfig *, int))rtems_bsdnet_loopattach, /* attach function */
    NULL,                           /* link to next interface */
    "127.0.0.1",                    /* IP address */
    "255.0.0.0",                    /* IP net mask */
};

struct rtems_bsdnet_config rtems_bsdnet_config = {
    &loopback_config,      /* Link to next interface */
    NULL,                  /* No BOOTP to get network configuration */
    0,                     /* If 0 then the network daemons will run at a */
                           /*   priority just less than the lowest-priority */
                           /*   EPICS scan thread. */
                           /* If non-zero then the network daemons will run */
                           /*   at this *RTEMS* priority */
    80*1024,               /* MBUF space */
    150*1024,              /* MBUF cluster space */
};


#include <stdio.h>
#include <rtems/error.h>
#include <epicsExit.h>

rtems_task
Init (rtems_task_argument ignored)
{
    extern void epicsRunLibComTests(void);
    rtems_time_of_day now;
    rtems_status_code sc;

    /*
     * Set time so that interval calculations work
     */
    if (rtems_clock_get (RTEMS_CLOCK_GET_TOD, &now) != RTEMS_SUCCESSFUL) {
        printf("***** Time/Date unknown -- set to arbitrary value.\n");
        now.year = 2001;
        now.month = 1;
        now.day = 1;
        now.hour = 0;
        now.minute = 0;
        now.second = 0;
        now.ticks = 0;
        if ((sc = rtems_clock_set (&now)) != RTEMS_SUCCESSFUL)
            printf ("***** Can't set time: %s\n", rtems_status_text (sc));
    }

    /*
     * Some tests need a 'network'
     */
    rtems_bsdnet_initialize_network();

    /*
     * Run the tests
     */
    epicsRunLibComTests();
    epicsExit(0);
}
