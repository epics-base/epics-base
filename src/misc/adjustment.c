/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* src/libCom/adjustment.c */

/* Author: Peregrine McGehee */

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

/* Up to now epicsShareThings have been declared as imports
 *   (Appropriate for other stuff)
 * After setting the following they will be declared as exports
 *   (Appropriate for what we implenment)
 */
#define epicsExportSharedSymbols
#include "adjustment.h"

epicsShareFunc size_t adjustToWorstCaseAlignment(size_t size)
{
    int align_size, adjust;
    struct test_long_word { char c; long lw; };
    struct test_double { char c; double d; };
    struct test_ptr { char c; void *p; };
    int test_long_size = sizeof(struct test_long_word) - sizeof(long);
    int test_double_size = sizeof(struct test_double) - sizeof(double);
    int test_ptr_size = sizeof(struct test_ptr) - sizeof(void *);
    size_t adjusted_size = size;
    
    /*
     * Use Jeff's alignment tests to determine worst case of long,
     * double or pointer alignment requirements.
    */
    align_size = test_long_size > test_ptr_size ?
                  test_long_size : test_ptr_size;
    
    align_size = align_size > test_double_size ?
                  align_size : test_double_size;
  
    /*
     * Increase the size to fit worst case alignment if not already
     * properly aligned.
     */ 
    adjust = align_size - size%align_size;
    if (adjust != align_size) adjusted_size += adjust;
    
    return (adjusted_size);
}
