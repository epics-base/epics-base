/*************************************************************************\
* Copyright (c) 2018 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution.
\*************************************************************************/

extern void epicsRunLinkTests(void);

int main(int argc, char **argv)
{
    epicsRunLinkTests();  /* calls epicsExit(0) */
    return 0;
}
