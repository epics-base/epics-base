
/**************************************************************************
 *
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

#include <stdio.h>
#include <string.h>

#ifdef vxWorks
#include <memLib.h>
#endif

#include "dbVarSub.h"
#include <dbStaticLib.h>

static int subst_total;
static struct var_sub *subst = (struct var_sub*)NULL;
static char* pattern;

static char* get_var(char**, char*);
static char* get_sub(char*, char*);

/* ------------------ variable substitution routines --------------*/
#ifdef vxWorks
static char* strdup(char*p) { return strcpy((char*)malloc(strlen(p)+1),p); }
#endif

void dbFreeSubst()
{
	int i;

	if(subst)
	{
		free(pattern);
		free(subst);
		subst = (struct var_sub*)NULL;
		subst_total = 0;
	}
}

long dbDoSubst(char* replace, int size, struct var_tree* par)
{
	/* perform substitution */
	char preal[VAR_MAX_SUB_SIZE];
	char pvar[VAR_MAX_SUB_SIZE];
	char *to,*from,*pp;
	char *hold = NULL;
	int i,num_chars;
	struct var_tree* test_var;
	struct var_tree my_tree;
	int l;

	my_tree.parent = par;
	l = strlen(replace);

	for(num_chars=0,to=preal,from=replace;from<=(replace+l);)
	{
		/* see if this is really a variable */
		if(from[0]==VAR_START_IND && from[1]==VAR_LEFT_IND)
		{
			/* found a variable */
			from += 2;
			if( !(pp=strchr(from,VAR_RIGHT_IND)) )
			{
				fprintf(stderr,
					"dbDoSubst: Improper variable specification: %s\n",
					from);
				return -1;
			}
			*pp++ = '\0'; /* clear the closing paren for variable */
			for(i=0;i<subst_total&&strcmp(subst[i].var,from);i++);
			if(i<subst_total)
			{
				/* found a substitution */
				strcpy(pvar,subst[i].sub);

				/* check for looping in substitution */
				my_tree.me=i;
				for(test_var=par;test_var;test_var=test_var->parent)
				{
					if(test_var->me==i)
					{
						fprintf(stderr,
							"dbDoSubst: recursive definition of variable %s\n",
							from);
						return -1;
					}
				}

				/* check for successful substitution */
				if(dbDoSubst(pvar,sizeof(pvar),&my_tree)<0) return -1;

				/* copy substitution to output string */
				for(hold=pvar;*to=*hold++;num_chars++,to++)
				{
					if(num_chars>size)
					{
						fprintf(stderr,
						 "dbDoSubst: substitution to long: %s), max=%d (r)\n",
							replace,size);
						return -1;
					}
				}
			}
			else
			{
				/* did not find a substitution */
#ifdef vxWorks
				fprintf(stderr,"dbDoSubst: did not find sub for %s\n",from);
#endif
				/* copy substitution to output string - this sucks */
				/* the "$()" part of the variable must be re-added */
				num_chars+=3; /* adjust for $() part */
				*to++='$'; /* put the variable $ back in */
				*to++='('; /* put the variable openning paren back in */
				for(hold=from;*to=*hold++;num_chars++,to++)
				{
					if(num_chars>size)
					{
						fprintf(stderr,
						 "dbDoSubst: substitution to long: %s), max=%d (e)\n",
							replace,size);
						return -1;
					}
				}
				*to++=')'; /* put the variable closing paren back in */
			}
			from = pp;
		}
		else
		{
			*to++ = *from++;
			if(num_chars++>size)
			{
				fprintf(stderr,
					"dbDoSubst: substitution to long for %s\n",
					replace);
				return -1;
			}
		}
	}
	strcpy(replace,preal);
	return 0;
}

long dbInitSubst(char* parm_pattern)
{
	char	    *pp,*hold;
	int		    rc,pi,pass;
	enum { var,sub } state;

	/* --------- parse the pattern --------- */

	rc=0;
	if(parm_pattern && *parm_pattern)
	{
		pattern = strdup(parm_pattern);

		dbFreeSubst();

		/* count the number of variables in the pattern (use the = sign) */
		for(subst_total=0,pp=pattern; *pp ;pp++)
		{
			/* find vars and subs */
			switch(*pp)
			{
			case '\\': pp++; break;
			case '=': subst_total++; break;
			case '\"': for(++pp;*pp!='\"';pp++) if(*pp=='\\') pp++; pp++; break;
			default: break;
			}
		}
		/* fprintf(stderr,"total = %d\n",subst_total); */
 
		/* allocate the substitution table */
		subst = (struct var_sub*)malloc( sizeof(struct var_sub)*subst_total );
 
		/* fill table from pattern - this is kind-of putrid */
		subst_total=0;
		pp=pattern;
		state=var;
		while(*pp)
		{
			switch(*pp)
			{
			case ' ':
			case ',':
			case '\t': pp++; break;
			case '\\': pp+=2; break;
			case '=':
			case '\"':
				pp=get_sub(subst[subst_total++].sub,pp);
				state=var;
				break;
			default:
				if(state==var)
				{
					pp=get_var(&subst[subst_total].var,pp);
					state=sub;
				}
				else
				{
					pp=get_sub(subst[subst_total++].sub,pp);
					state=var;
				}
				break;
			}
		}

		/* debug code */
		/*
		for(pi=0;pi<subst_total;pi++)
		{
			printf("table[%d]=(%s,%s)\n",pi,subst[pi].var,subst[pi].sub);
		}
		*/

		/* resolve the multiple substitutions now */
		for(pi=0;pi<subst_total;pi++)
		{
			if(dbDoSubst(subst[pi].sub,VAR_MAX_SUB_SIZE,(struct var_tree*)NULL)<0)
			{
				fprintf(stderr, "dbInitSubst: failed to build variable substitution table (%s)\n",subst[pi].sub);
				rc = -1;
			}
		}

		/* more debug code */
		/*
		for(pi=0;pi<subst_total;pi++)
		{
			printf("table[%d]=(%s,%s)\n",pi,subst[pi].var,subst[pi].sub);
		}
		*/
	}
	else
	{
		subst_total=0;
		subst=(struct var_sub*)NULL;
    }
	return(rc);
}

/* put the pointer to the variable in "from" into "to" */
static char* get_var(char** to, char* from)
{
	char* pp;

	pp = strpbrk(from," \t=");
	*pp = '\0';
	pp++;
	/* fprintf(stderr,"get_var: (%s)\n",from); */
	*to=from;
	return pp;
}

/* copy the substitution in "from" into "to" */
static char* get_sub(char* to, char* from)
{
	char *pp,*hold;
	char* cp = to;

	for(pp=from;*pp==' ' || *pp=='\t' || *pp=='=';pp++);

	if(*pp=='\"')
	{
		for(++pp;*pp!='\"';pp++)
		{
			if(*pp=='\\') pp++;
			else *cp++ = *pp;
		}
		*cp='\0';
		/* fprintf(stderr,"get_sub: quote (%s)\n",to); */
		pp++;
	}
	else
	{
		for(hold=pp;*hold && *hold!=',';hold++);
		if(*hold)
		{
			*hold = '\0';
			hold++;
			/* fprintf(stderr,"get_sub: regular (%s)\n",pp); */
		}

		strcpy(to,pp);
		pp=hold;
	}
	return pp;
}
