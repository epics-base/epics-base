/* calcTest.c */
/* share/src/util   $Id$ */

/* calcTest.c - calcPerform Test Program  
 *
 *	Author:		Janet Anderson
 *	Date:		02-05-92
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	02-10-92	jba	initial version
 * .02	06-16-92	jba	Increased dim of rpbuf to hold double constants in expression
 *
 */

#ifdef vxWorks
#include        <vxWorks.h>
#include        <stdioLib.h>
#include        <strLib.h>
#else
#include        <stdio.h>
#include        <string.h>
#endif

#ifndef ERROR
#define ERROR	-1
#endif
#ifndef OK
#define OK	0
#endif

#define PUT	0
#define GET	1
#define MAXLINE 150

char		errmsg[MAXLINE];

long calcPerform();
long postfix();
void calcTest();
long doTestLine();

#ifndef vxWorks
main()
{

	calcTest();

}
#endif

void calcTest()
{
	char str[MAXLINE];

	char expression[MAXLINE];
	char parmList[MAXLINE];
	char value[MAXLINE];
	int  fieldCount;
	long status;
	double	parm[12];
	short	i;

	/* get test line */
	while ( gets(str)) {
		/* print and skip null lines and comment lines */
		if ( str[0]==NULL || str[0] == '!' ) {
			printf("%s\n",str);
			continue;
		}

		/* parse input line for fields */
		fieldCount=sscanf(str," \"%[^\"]\" \"%[^\"]\" %s ",
			expression,parmList,value);

		/* check field count */
		if ( fieldCount < 2 || fieldCount > 3 ) {
			printf("\n%s\n",str);
			printf ("***** ERROR ***** input field count %d incorrect\n",fieldCount);
			continue;
		}

		/* parse parmList for parameters */
		status=sscanf(parmList,"%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf",
				&parm[0],&parm[1],&parm[2],&parm[3],&parm[4],&parm[5],
				&parm[6],&parm[7],&parm[8],&parm[9],&parm[10],&parm[11]);
		if( status == EOF ){
			printf("\n%s\n",str);
			printf ("***** ERROR ***** Error in parameter list %s \n",parmList);
			continue;
		}

		/* zero out rest of parameters */
		for (i=status; i<12; i++) parm[i]=0.;


		/* execute test line */
		status=doTestLine(expression,parm,value,fieldCount);
		if ( status ){
			printf("\n%s\n",str);
			printf ("***** ERROR ***** status=%d %s\n",status,errmsg);
		}

	}
	printf ("\ncalcTest ALL DONE\n\n\n");
}

long doTestLine(expression,parm,value,fieldCount)
char	expression[];
double	parm[12];
char	value[];
int	fieldCount;
{
	long	status=0;
	short	error_number=0;
        /* rpbuf dimension must be >= 10*(dimension_of_CALC_field/2)   */
        /* since each constant in the expression now takes 9 bytes (chars):   1(code)+8(double) */
	char	rpbuf[184];
	char	valString[80];
	double	val;

	/* convert an algebraic expression to symbolic postfix */
	status=postfix(expression,rpbuf,&error_number);
	if(status || error_number) {
		sprintf(errmsg,"postfix error:  status=%ld error_number=%d \n",status,error_number);
		return ERROR;
	}

	/* perform calculation */
	status=calcPerform(parm,&val,rpbuf);
	if(status) {
		sprintf(errmsg,"calcPerform error:  status=%ld \n",status);
		return ERROR;
	}

	/* convert  calcPerform result to string */
	sprintf(valString,"%-g",val);

	/* verify calcPerform result*/
	if ( fieldCount == 3 ){

		status=strcmp(value,valString);
		if (status) {
			sprintf(errmsg,"verify failed: calcPerform result: val=%s",valString);
			return ERROR;
		}
	}
	else printf("calcPerform result: %s\n",valString);

	return OK;
}

