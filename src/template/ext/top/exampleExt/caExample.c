/* SPDX-FileCopyrightText: 1998 Argonne National Laboratory */

/* SPDX-License-Identifier: EPICS */

/*caExample.c*/
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cadef.h"

int main(int argc,char **argv)
{
    double	data;
    chid	mychid;

    if(argc != 2) {
	fprintf(stderr,"usage: caExample pvname\n");
	exit(1);
    }
    SEVCHK(ca_context_create(ca_disable_preemptive_callback),"ca_context_create");
    SEVCHK(ca_create_channel(argv[1],NULL,NULL,10,&mychid),"ca_create_channel failure");
    SEVCHK(ca_pend_io(5.0),"ca_pend_io failure");
    SEVCHK(ca_get(DBR_DOUBLE,mychid,(void *)&data),"ca_get failure");
    SEVCHK(ca_pend_io(5.0),"ca_pend_io failure");
    printf("%s %f\n",argv[1],data);
    return(0);
}
