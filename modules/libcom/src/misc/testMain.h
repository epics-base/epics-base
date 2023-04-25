/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_testMain_H
#define INC_testMain_H

/*!
 * \file testMain.h
 *
 * \brief This header defines a platform independent macro for defining a test
 * main function, in pure test programs.
 *
 * A pure test program cannot take any arguments since it must be fully
 * automatable.  If your program needs to use argv/argc, it may be doing
 * measurements not unit and/or regression testing.  On Host architectures
 * these programs needs to be named main and take dummy argc/argv args,
 * but on vxWorks and RTEMS they must be named as the test program.
 *
 * \section Example
 * \code{.cpp}
 *     #include "testMain.h"
 *     #include "epicsUnitTest.h"
 *
 *     MAIN(myProgTest) {
 *         testPlan(...);
 *         testOk(...)
 *         return testDone();
 *     }
 * \endcode
 */

/*!
 * \def MAIN
 * \brief Macro which defines a main function for your test program.  Some platforms will name this function main(), others prog().
 * \param prog Name of the test program.
 **/
#if defined(__rtems__)
  #ifdef __cplusplus
    #define MAIN(prog) extern "C" int prog(void); extern "C" int main() __attribute__((weak, alias(#prog))); extern "C" int prog(void)
  #else
    #define MAIN(prog) int prog(); int main() __attribute__((weak, alias(#prog))); int prog()
  #endif
#elif defined(vxWorks)
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
