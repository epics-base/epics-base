/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_testMain_H
#define INC_testMain_H

/* This header defines a convenience macro for use by pure test programs.
 * A pure test program cannot take any arguments since it must be fully
 * automatable.  If your program needs to use argv/argc, it may be doing
 * measurements not unit and/or regression testing.  On Host architectures
 * these programs needs to be named main and take dummy argc/argv args,
 * but on vxWorks and RTEMS they must be named as the test program.
 *
 * Use this macro as follows:
 *
 *     #include "testMain.h"
 *     #include "epicsUnitTest.h"
 *     
 *     MAIN(myProgTest) {
 *         testPlan(...);
 *         testOk(...)
 *         return testDone();
 *     }
 */

#if defined(vxWorks) || defined(__rtems__)
  #ifdef __cplusplus
    #define MAIN(prog) extern "C" int prog(void)
  #else
    #define MAIN(prog) int prog()
  #endif
#else
  #ifdef __cplusplus
    #define MAIN(prog) int main(int /*argc*/, char * /*argv*/ [] )
  #else
    #define MAIN(prog) int main(int argc, char *argv[] )
  #endif
#endif


#endif /* INC_testMain_H */
