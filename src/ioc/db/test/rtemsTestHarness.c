/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/


#ifdef __rtems__

#include <stdio.h>
#include <envDefs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#endif /* __rtems__ */

extern void epicsRunDbTests(void);

int main(int argc, char **argv)
{
#ifdef __rtems__
    struct stat s;
    printf("Try to create /tmp\n");
    umask(0);
    if(mkdir("/tmp", 0777)!=0)
        perror("Can't create /tmp");
    if(stat("/tmp", &s)==0) {
        printf("Stat /tmp: %o %u,%u\n", s.st_mode, s.st_uid, s.st_gid);
    }
    epicsEnvSet("TMPDIR","/tmp");
    {
        char name[40];
        if(getcwd(name,40))
            printf("Running from %s\n", name);
        else
            printf("Can't determine PWD");
    }
#endif

    epicsRunDbTests();  /* calls epicsExit(0) */
    return 0;
}
