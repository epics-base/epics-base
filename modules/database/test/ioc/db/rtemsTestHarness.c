/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

extern void epicsRunDbTests(void);

int main(int argc, char **argv)
{
    epicsRunDbTests();  /* calls epicsExit(0) */
    return 0;
}
