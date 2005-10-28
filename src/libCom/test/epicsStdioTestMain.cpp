/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsStdioTestMain.cpp
 *
 *      Author  Marty Kraimer
 */

extern "C" {
int epicsStdioTest (const char *report);
}

int main ( int argc, char *argv[] )
{
    const char *report = "";
    if(argc>1) report = argv[1];

    epicsStdioTest (report);
    return 0;
}
