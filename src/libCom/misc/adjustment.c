/* src/libCom/adjustment.c */

/* Author: Peregrine McGehee Date: 21NOV1997 */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/*
 *
 * Modification Log:
 * -----------------
 * .01  11-21-97	pmm	Initial Implementation	
 */

#ifdef vxWorks
#include <vxWorks.h>
#include <taskLib.h>
#include "fast_lock.h"
#endif

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

epicsShareFunc size_t epicsShareAPI adjustToWorstCaseAlignment(size_t size)
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
