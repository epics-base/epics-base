/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "epicsExit.h"

int
main(int argc, char **argv)
{
    extern void epicsRunLibComTests(void);
    epicsRunLibComTests();
    epicsExit(0);
    return 0;
}
