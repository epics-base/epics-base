/* devSymbTest.c - Test routines for vxWorks global var device support */

/* $Id$
 *
 * Author:	William Lupton (Keck)
 * Date:	8 June 1996
 */

/* modification history:
 * $Log$
 * Revision 1.1  1996/06/09 00:52:25  wlupton
 * initial insertion
 *
 */

/*
DESCRIPTION:

This module contains routines for testing vxWorks global variable 
device support.
*/

#include        <vxWorks.h>
#include        <sysSymTbl.h>

#include        <ctype.h>
#include        <stdio.h>
#include        <string.h>

#include        <dbDefs.h>
#include        <link.h>

/* global variables for testing */

/* long scalar */
long testLongScalar = 42;

/* long array */
long testLongArray[] = {0,1,2,3,4,5,6,7,8,9};

/* double scalar */
double testDoubleScalar = 125.0;

/* double array */
double testDoubleArray[] = {20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0};

/* string */
char testString[] = "0123456789";

/* structure and variables referencing its fields */
struct {
    long xxxx;
    double yyyy[3];
    char zzzz[80];
} testStruct = {35, {1.0,2.0,3.0}, "hello dolly"};

long *testLongPtr = &testStruct.xxxx;
double *testDoublePtr = testStruct.yyyy;
char *testStringPtr = testStruct.zzzz;

/* routines for starting and stopping auto-change of values */
long testRunning = 0;

int testStart() {
    int i, j;

    testRunning = 1;

    for ( i = 0; testRunning; i++ ) {

	/* change global data values */
 	testLongScalar++;
	for ( j = 0; j < sizeof( testLongArray ) / sizeof( long ); j++ )
	    testLongArray[j]++;
	testDoubleScalar++;
	for ( j = 0; j < sizeof( testDoubleArray ) / sizeof( double ); j++ )
	    testDoubleArray[j]++;
	for ( j = 0; j < strlen( testString ) - 1; j++ )
	    testString[j] = testString[j+1];
 	testString[j] = testString[0];
	testStruct.xxxx++;
	testStruct.yyyy[0]++;
	testStruct.yyyy[1]++;
	testStruct.yyyy[2]++;
	if ( i % 2 )
	    strcpy( testStruct.zzzz, "hello dolly" );
	else
	    strcpy( testStruct.zzzz, "dolly hello" );

	/* wait a second */
	taskDelay( sysClkRateGet() );
    }
    printf( "test stopped\n" );
    return 0;
}

int testStop() {
    testRunning = 0;
    return 0;
}

