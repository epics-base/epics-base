/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsMathTestMain.cpp
 *
 *      Author  Marty Kraimer
 */

extern "C" {
int epicsMathTest ( void );
}

int main ( int  , char *[] )
{
    epicsMathTest ();
    return 0;
}
