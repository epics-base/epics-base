
/**************************************************************************
 *	$Id$
 *     Author:	Jim Kowalkowski
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	10-29-93	jbk	initial version
 *
 ***********************************************************************/

#define VAR_LEFT_IND '('
#define VAR_RIGHT_IND ')'
#define VAR_START_IND '$'
#ifdef vxWorks
#define VAR_MAX_SUB_SIZE 200
#define VAR_MAX_VAR_STRING 1500
#define VAR_MAX_VARS 100
#else
#define VAR_MAX_SUB_SIZE 300
#define VAR_MAX_VAR_STRING 50000
#define VAR_MAX_VARS 700
#endif
 
struct var_sub {
    char *var;
    char sub[VAR_MAX_SUB_SIZE];
};
           
struct var_tree {
    int me;
    struct var_tree* parent;
};

long dbDoSubst(char* replace, int replace_size, struct var_tree* par);
long dbInitSubst(char* parm_pattern);
void dbFreeSubst();

